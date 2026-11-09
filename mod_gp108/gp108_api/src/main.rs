use std::io;
use std::fs::OpenOptions;
use std::os::fd::AsRawFd;
use libc;

#[repr(C)]
struct Gp108Info {
    vendor: u32,
    device: u32,
    gpu_id: u32,
    revision: u32,
}

const fn ior_r(arg1: u32, arg2: u32, arg3: usize) -> u32 {
    (1u32 << 31) | ((arg1) << 8) | ((arg2) << 0) | ((arg3 as u32 ) << 16)
}

const GP108_R: u32 = ior_r(b'G' as u32, 0x00 as u32, core::mem::size_of::<Gp108Info>());

fn main() -> io::Result<()> {
    let file = OpenOptions::new().read(true).open("/dev/gp108")?;
    let mut gp108_info = Gp108Info {
        vendor: 0,
        device: 0,
        gpu_id: 0,
        revision: 0,
    };
    let fd = file.as_raw_fd();
    let gp108_info_rp : *mut Gp108Info = &mut gp108_info;
    let res = unsafe{libc::ioctl(fd,GP108_R as libc::c_ulong, gp108_info_rp)};
    if res < 0 
    {
        return Err(io::Error::last_os_error());
    }
    println!("GPU_INFO\n");
    println!("vendor   : {:04x}", gp108_info.vendor);
    println!("device   : {:04x}", gp108_info.device);
    println!("gpu_id   : {:02x}", gp108_info.gpu_id);
    println!("revision : {:02x}", gp108_info.revision);
    Ok(())
}

/*
#define GPU_INFO _IOR('G', 0x00, struct gp108_info)

Expands to:

(((2U) << (((0+8)+8)+14)) | ((('G')) << (0+8)) | (((0x00)) << 0) | ((((sizeof(struct gp108_info)))) << ((0+8)+8)))

*/
