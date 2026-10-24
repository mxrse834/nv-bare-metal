#!/usr/bin/env python3
"""Nouveau source lookup and call-graph CLI.

Run from the Linux source repository root:
  python3 nuvo-map/nvmap.py <search text>
  python3 nuvo-map/nvmap.py graph FUNCTION [--depth N] [--structs] [--direct-only]
"""

import argparse
import csv
import os
import re
import subprocess
import sys
from collections import defaultdict, deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent


def read_tsv(name):
    try:
        with (ROOT / name).open(newline="") as f:
            return list(csv.DictReader(f, delimiter="\t"))
    except FileNotFoundError:
        return []


def read_occurrences(name):
    try:
        with (ROOT / name).open(newline="") as f:
            return [
                {"name": r[0], "location": r[1], "context": r[2] if len(r) > 2 else ""}
                for r in csv.reader(f, delimiter="\t")
                if len(r) >= 2
            ]
    except FileNotFoundError:
        return []


def show(title, rows, fields, query, limit=30):
    matches = [
        r for r in rows
        if any(query.lower() in str(r.get(k, "")).lower() for k in fields)
    ]
    if not matches:
        print(f"\n=== {title}: no matches ===")
        return
    print(f"\n=== {title}: {len(matches)} matches ===")
    for r in matches[:limit]:
        print(" | ".join(f"{k}={r[k]}" for k in fields if r.get(k, "")))
    if len(matches) > limit:
        print(f"... {len(matches) - limit} more; refine your search.")


def lookup(query):
    show("CHIPSET → CONSTRUCTOR", read_tsv("constructor-defs.tsv"),
         ["chipset", "member", "constructor", "definition_file", "definition_line"], query)
    show("CONSTRUCTOR → CALLBACK", read_tsv("constructor-callbacks.tsv"),
         ["constructor", "table", "field", "callback", "definition", "table_location"], query)
    show("CALLBACK TABLES", read_tsv("callback-bindings.tsv"),
         ["struct_type", "table_name", "field", "assigned_name", "location"], query)
    show("LAYOUT", read_tsv("layout.tsv"),
         ["kind", "type", "data_type", "member", "capacity", "line"], query)
    show("IDENTIFIER OCCURRENCES", read_occurrences("identifiers.tsv"),
         ["name", "location", "context"], query)
    show("SYMBOL OCCURRENCES", read_occurrences("symbol-uses.tsv"),
         ["name", "location", "context"], query)


def read_direct_edges(path):
    """Read the name-only edges from the existing Graphviz call graph."""
    adj = defaultdict(set)
    edge_re = re.compile(r'^\s*"([^"\n]+)"\s*->\s*"([^"\n]+)"')
    with path.open(errors="replace") as f:
        for line in f:
            m = edge_re.match(line)
            if m:
                adj[m.group(1)].add(m.group(2))
    return adj


def load_function_index(path):
    """Read the ctags-style functions.txt index; it has no header row."""
    funcs = defaultdict(list)
    if not path.exists():
        return funcs
    with path.open(errors="replace") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) < 4:
                continue
            name, relpath, pattern, kind = p[:4]
            # Ctags versions format the kind as either 'f' or 'ftyperef:...'.
            if not kind.startswith("f"):
                continue
            if not re.match(r"^[A-Za-z_]\w*$", name):
                continue
            funcs[name].append((relpath, pattern))
    return funcs


def ctags_signature(pattern):
    # Typical pattern: /^function_name(args)$/;"
    if pattern.startswith("/^") and "$/;" in pattern:
        return pattern[2:pattern.rfind("$/;")]
    return ""


def _extract_function_text(source_from_tag):
    """Extract a C function definition, handling braces in comments/strings."""
    state = "normal"
    escaped = False
    opening = -1
    depth = 0
    i = 0
    while i < len(source_from_tag):
        c = source_from_tag[i]
        n = source_from_tag[i + 1] if i + 1 < len(source_from_tag) else ""
        if state == "line_comment":
            if c == "\n":
                state = "normal"
        elif state == "block_comment":
            if c == "*" and n == "/":
                state = "normal"
                i += 1
        elif state in ("string", "char"):
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif (state == "string" and c == '"') or (state == "char" and c == "'"):
                state = "normal"
        else:
            if c == "/" and n == "/":
                state = "line_comment"
                i += 1
            elif c == "/" and n == "*":
                state = "block_comment"
                i += 1
            elif c == '"':
                state = "string"
                escaped = False
            elif c == "'":
                state = "char"
                escaped = False
            elif c == ";" and opening < 0:
                # A declaration/prototype, not a definition.
                return None
            elif c == "{":
                if opening < 0:
                    opening = i
                depth += 1
            elif c == "}" and opening >= 0:
                depth -= 1
                if depth == 0:
                    return source_from_tag[:i + 1]
        i += 1
    return None


class SourceReader:
    def __init__(self, function_index):
        self.function_index = function_index
        self.file_cache = {}
        self.body_cache = {}

    def _file_lines(self, relpath):
        if relpath not in self.file_cache:
            path = REPO / relpath
            if not path.exists():
                self.file_cache[relpath] = []
            else:
                try:
                    self.file_cache[relpath] = path.read_text(errors="replace").splitlines()
                except OSError:
                    self.file_cache[relpath] = []
        return self.file_cache[relpath]

    def bodies(self, name):
        if name in self.body_cache:
            return self.body_cache[name]
        found = []
        seen = set()
        for relpath, pattern in self.function_index.get(name, []):
            lines = self._file_lines(relpath)
            if not lines:
                continue
            sig = ctags_signature(pattern).strip()
            starts = []
            if sig:
                starts = [i for i, line in enumerate(lines) if line.strip() == sig]
            if not starts:
                # Fallback for ctags patterns that don't exactly match source formatting.
                rx = re.compile(r"^\s*" + re.escape(name) + r"\s*\(")
                starts = [i for i, line in enumerate(lines) if rx.match(line)]
            for start in starts:
                chunk = _extract_function_text("\n".join(lines[start:]))
                if chunk:
                    key = (relpath, start)
                    if key not in seen:
                        found.append((relpath, start + 1, chunk))
                        seen.add(key)
                    break
        self.body_cache[name] = found
        return found


def load_field_types(path):
    """Map (struct type, field) -> pointed-to struct type from fields.txt."""
    result = {}
    if not path.exists():
        return result
    with path.open(errors="replace") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) < 6:
                continue
            field_name = p[0]
            scope = next((x for x in p[4:] if x.startswith("struct:")), "")
            typeref = next((x for x in p[4:] if x.startswith("typeref:")), "")
            if not scope or not typeref:
                continue
            owner = scope[len("struct:"):].split("::")[-1]
            m = re.search(r"\bstruct\s+([A-Za-z_]\w*)", typeref)
            if m:
                result[(owner, field_name)] = m.group(1)
    return result


def load_callback_targets(path):
    """Map (callback-table struct type, field) to possible table assignments."""
    result = defaultdict(set)
    for row in read_tsv(path.name):
        table_type = row.get("struct_type", "").removeprefix("struct ").strip()
        field = row.get("field", "").strip()
        table = row.get("table_name", "").strip()
        assigned = row.get("assigned_name", "").strip()
        assigned = assigned.lstrip("&").strip()
        if (table_type and field and table and
                re.match(r"^[A-Za-z_]\w*$", assigned)):
            result[(table_type, field)].add((table, assigned))
    return result


def variable_struct_types(body):
    """A conservative local/parameter declaration scan for 'struct T *var'."""
    found = {}
    for m in re.finditer(r"\bstruct\s+([A-Za-z_]\w*)\s*(?:\*\s*)+([A-Za-z_]\w*)\b", body):
        found[m.group(2)] = m.group(1)
    return found


def callback_edges_for(name, reader, field_types, callback_targets):
    """Resolve obj->table_field->callback() into possible assigned functions."""
    result = set()
    call_re = re.compile(
        r"\b([A-Za-z_]\w*)\s*->\s*([A-Za-z_]\w*)\s*->\s*([A-Za-z_]\w*)\s*\("
    )
    for _path, _line, body in reader.bodies(name):
        var_types = variable_struct_types(body)
        for m in call_re.finditer(body):
            var, table_field, callback_field = m.groups()
            owner_type = var_types.get(var)
            if not owner_type:
                continue
            table_type = field_types.get((owner_type, table_field))
            if not table_type:
                continue
            for table_name, target in callback_targets.get((table_type, callback_field), ()):
                result.add((target, "callback", f"candidate: {table_name}.{callback_field}"))
    return result


def source_structs(reader, name):
    structs = set()
    for _path, _line, body in reader.bodies(name):
        structs.update(re.findall(r"\bstruct\s+([A-Za-z_]\w*)\b", body))
    return structs


def dot_quote(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def build_graph(root, direct, reader, field_types, callback_targets, max_depth, with_callbacks, with_structs):
    queue = deque([(root, 0)])
    min_depth = {root: 0}
    expanded = set()
    graph_edges = set()  # (source, target, kind, label)
    structs_by_function = defaultdict(set)
    cutoff = set()
    callback_edge_count = 0

    while queue:
        name, depth = queue.popleft()
        if name in expanded:
            continue
        expanded.add(name)

        if with_structs:
            structs_by_function[name] = source_structs(reader, name)

        outgoing = {(child, "direct", "") for child in direct.get(name, ())}
        if with_callbacks:
            outgoing.update(callback_edges_for(name, reader, field_types, callback_targets))

        if max_depth is not None and depth >= max_depth:
            if outgoing:
                cutoff.add(name)
            continue

        for child, kind, label in outgoing:
            graph_edges.add((name, child, kind, label))
            if kind == "callback":
                callback_edge_count += 1
            nd = depth + 1
            if child not in min_depth or nd < min_depth[child]:
                min_depth[child] = nd
                queue.append((child, nd))

    return min_depth, graph_edges, structs_by_function, cutoff, callback_edge_count


def render_dot(root, depths, edges, structs_by_function, cutoff, with_structs):
    nodes = set(depths)
    for source, target, _kind, _label in edges:
        nodes.add(source)
        nodes.add(target)
    lines = [
        "digraph NouveauGraph {",
        '  graph [rankdir=LR, overlap=false, splines=true, label="Nouveau dependency graph", labelloc=t];',
        '  node [shape=box, style="rounded,filled", fillcolor="white", fontsize=9];',
        '  edge [fontsize=8];',
    ]
    struct_nodes = set()
    struct_edges = set()
    if with_structs:
        for fn, types in structs_by_function.items():
            for typ in types:
                struct_node = "struct:" + typ
                struct_nodes.add(struct_node)
                struct_edges.add((fn, struct_node))
    for node in sorted(nodes):
        label = node
        if node in cutoff:
            label += "\\n[depth limit]"
            fill = "lightgoldenrod1"
        elif not any(e[0] == node for e in edges):
            fill = "honeydew2"
        elif node == root:
            fill = "lightblue"
        else:
            fill = "white"
        lines.append(f'  "{dot_quote(node)}" [label="{dot_quote(label)}", fillcolor="{fill}"];')
    for struct_node in sorted(struct_nodes):
        typ = struct_node.split(":", 1)[1]
        lines.append(f'  "{dot_quote(struct_node)}" [label="struct {dot_quote(typ)}", shape=component, fillcolor="lavender"];')
    for source, target, kind, label in sorted(edges):
        if kind == "callback":
            lines.append(
                f'  "{dot_quote(source)}" -> "{dot_quote(target)}" '
                f'[style=dashed, color=darkorange3, label="{dot_quote(label)}"];'
            )
        else:
            lines.append(f'  "{dot_quote(source)}" -> "{dot_quote(target)}" [color=gray35];')
    if with_structs:
        for source, target in sorted(struct_edges):
            lines.append(
                f'  "{dot_quote(source)}" -> "{dot_quote(target)}" '
                '[style=dotted, color=slateblue, label="uses type", constraint=false];'
            )
    lines.append("}")
    return "\n".join(lines) + "\n"


def graph_command(argv):
    parser = argparse.ArgumentParser(
        prog="nvmap.py graph",
        description="Render a bounded or full-depth Nouveau call/dependency graph.",
    )
    parser.add_argument("function", help="exact function symbol to start from")
    parser.add_argument("--depth", type=int, default=None,
                        help="maximum call depth (default: expand until leaves; cycles stop automatically)")
    parser.add_argument("--structs", action="store_true",
                        help="add structure-type nodes and dotted 'uses type' edges")
    parser.add_argument("--direct-only", action="store_true",
                        help="disable inferred callback-candidate edges")
    parser.add_argument("-o", "--output", default=None,
                        help="output SVG path (default: nuvo-map/graphs/FUNCTION.svg); use .dot to write DOT only")
    args = parser.parse_args(argv)
    if args.depth is not None and args.depth < 0:
        parser.error("--depth must be >= 0")

    graph_file = ROOT / "all-calls.dot"
    if not graph_file.exists():
        parser.error(f"missing {graph_file}; generate the existing all-calls.dot first")
    direct = read_direct_edges(graph_file)
    funcs = load_function_index(ROOT / "functions.txt")
    if args.function not in direct and args.function not in funcs:
        parser.error(f"'{args.function}' not found in all-calls.dot or functions.txt")

    reader = SourceReader(funcs)
    field_types = load_field_types(ROOT / "fields.txt")
    callback_targets = load_callback_targets(ROOT / "callback-bindings.tsv")
    depths, edges, structs_by_function, cutoff, callback_count = build_graph(
        args.function, direct, reader, field_types, callback_targets,
        args.depth, not args.direct_only, args.structs,
    )
    dot_text = render_dot(args.function, depths, edges, structs_by_function, cutoff, args.structs)

    safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "_", args.function)
    output = Path(args.output) if args.output else ROOT / "graphs" / f"{safe_name}.svg"
    if not output.is_absolute():
        output = REPO / output
    output.parent.mkdir(parents=True, exist_ok=True)
    dot_path = output if output.suffix.lower() == ".dot" else output.with_suffix(".dot")
    dot_path.write_text(dot_text)

    print(f"Root: {args.function}")
    print(f"Reachable nodes: {len(depths)}")
    print(f"Edges: {len(edges)} ({sum(1 for e in edges if e[2] == 'direct')} direct, "
          f"{sum(1 for e in edges if e[2] == 'callback')} callback candidates)")
    if args.depth is None:
        print("Depth: full (all discovered reachable nodes; cycles de-duplicated)")
    else:
        print(f"Depth: {args.depth} (nodes at the limit are not expanded)")
    if args.structs:
        print(f"Structure types: {len({t for ts in structs_by_function.values() for t in ts})}")
    if args.direct_only:
        print("Callback resolution: disabled")
    else:
        print("Callback resolution: conservative candidates; not chipset-specific proof")
    print(f"DOT: {dot_path}")

    if output.suffix.lower() == ".dot":
        return 0
    try:
        subprocess.run(["dot", "-Tsvg", str(dot_path), "-o", str(output)], check=True)
    except FileNotFoundError:
        print("Graphviz 'dot' was not found; DOT was saved, but SVG was not rendered.", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as e:
        print(f"Graphviz failed (exit {e.returncode}); DOT was saved.", file=sys.stderr)
        return e.returncode or 1
    print(f"SVG: {output}")
    return 0


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "graph":
        return graph_command(sys.argv[2:])
    if len(sys.argv) < 2:
        print("Usage: python3 nuvo-map/nvmap.py <name>\n"
              "       python3 nuvo-map/nvmap.py graph FUNCTION [--depth N] [--structs] [--direct-only]")
        return 1
    lookup(" ".join(sys.argv[1:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
