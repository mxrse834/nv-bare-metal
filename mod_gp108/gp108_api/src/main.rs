#[repr(C)]
struct Gp108Info {
    vendor: u32,
    device: u32,
    gpu_id: u32,
    revision: u32,
}

const fn ior_r(arg1: u32, arg2: u32, arg3: usize) -> u32 {
    ((1u32 << 31) | ((arg1) << 8) | ((arg2) << 0) | ((arg3) << 16))
}

const GP108_R: u32 = IOR_r(b'G' as u32, 0x00 as u32, core::mem::size_of::<Gp108_Info>());

fn main() -> io::result<()> {
    let file = OpenOptions::new().read(true).open("/dev/gp108")?;
    let gp108_info = Gp108Info {
        vendor: 0,
        device: 0,
        gpu_id: 0,
        revision: 0,
    };
}

/*
#define GPU_INFO _IOR('G', 0x00, struct gp108_info)

Expands to:

(((2U) << (((0+8)+8)+14)) | ((('G')) << (0+8)) | (((0x00)) << 0) | ((((sizeof(struct gp108_info)))) << ((0+8)+8)))

*/
