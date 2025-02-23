vistaspctl
Basic utility to tell SpLdr to start and stop SpSys on Vista (and 7.)

On 7, you can just enable and disable the service from the services.msc GUI.
This is because SpLdr keeps a count of all active handles to the SpDevice, and
if that count reaches zero, the SpSys driver is automatically stopped.

On Vista, you are not so lucky, as this mechanism does not exist and SlSvc does
not attempt to get the driver unloaded. However, there are IOCTLs in SpLdr's
driver, seemingly unused, that trigger the driver to start and stop independent
of SlSvc. They can also be used to start the driver when DisableAutoStart is
enabled in the SpLdr driver registry entry, which usually prevents SlSvc from
functioning.

Commands:

  vistaspctl.exe
    Prints the current status of SpSys.
  
  vistaspctl.exe stop
    Stops the SlSvc service and the SpSys driver.
  
  vistaspctl.exe start
    Starts the SpSys driver. (Vista only, performs startsvc on 7)
  
  vistaspctl.exe startsvc
    Starts the SlSvc service and the SpSys driver.

IOCTLs:

  Device: \\.\SpDevice
  Driver: spldr.sys
  
  SPLDR_IOCTL_GET_DRIVE_STATUS = 0x80006008
    Input buffer: NULL
    Output buffer: 1 byte, returns a byte that is 0x1 if running, 0x0 if not.
    Permissions: GENERIC_READ
  
  SPLDR_IOCTL_START_DRIVER_VISTA = 0x8000a000
    Input buffer: NULL
    Output buffers: 4 bytes, returns an NT status if load fails.
    Permissions: GENERIC_WRITE
    Note: Output buffer is not populated if it was successful.
  
  SPLDR_IOCTL_STOP_DRIVER_VISTA = 0x8000a004
    Input buffer: NULL
    Output buffers: 4 bytes, returns an NT status if stop fails.
    Permissions: GENERIC_WRITE
    Note: Output buffer is not populated if it was successful.
  
  The latter two IOCTLs don't exist in 7's SpLdr.

Building:

  1. Install the 7 DDK. Older versions might work.
  2. Run the Vista "x86 Free Build Environment" Command Prompt.
  3. Type "bld".

This tool was intended solely to research an outdated version of the kernel and
the systems that protect older versions of the operating system from
unauthorised use, and not to aid in any unauthorized activity.
