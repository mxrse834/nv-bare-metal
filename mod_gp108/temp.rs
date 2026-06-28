use kernel::prelude::*;

module!{
type : Hello,
name : "hello",
author : "mxrse" ,
description : "hello kernel module"
}


//ThisModule is the kernels representation of the loaded module

impl kernel::Module for Hello
    {
    fn(_module : &'static ThisModule) -> Result<Self>
        {
            pr_info!("Kernel init");
        }
    }

impl Drop for hello
{
    fn drop(&mut self) 
    {
        pr_info!("bye");
    }
}
  
