# GDB script for debugging boot process
target remote localhost:1234

# Set architecture
set architecture i8086

# Break at boot sector start
break *0x7c00

# Break at stage2 start
break *0x7e00

# Break at kernel start
break *0x1000

# Continue
continue

# Examine registers
info registers

# Disassemble
x/20i $pc

# If we hit bootloader, step through a few instructions
define boot_step
  stepi
  x/1i $pc
  info registers cs ip sp
end
