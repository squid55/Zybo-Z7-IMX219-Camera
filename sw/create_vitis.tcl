##########################################################################
## Vitis Project Creation Script
## CNN MNIST + Pcam-5C application
##########################################################################

set xsa_file  "/home/hyeonjun/cnn_pcam_project/cnn_pcam.xsa"
set ws_dir    "/home/hyeonjun/cnn_pcam_project/vitis_ws"
set app_name  "cnn_mnist_app"
set plat_name "cnn_pcam_platform"
set sw_src    "/home/hyeonjun/cnn_pcam_project/sw/src"

# 1. Set workspace
setws $ws_dir

# 2. Create platform from XSA
platform create -name $plat_name -hw $xsa_file -proc ps7_cortexa9_0 -os standalone
platform generate

# 3. Create application project
app create -name $app_name -platform $plat_name -domain standalone_domain -template "Empty Application" -lang c++

# 4. Import source files
importsources -name $app_name -path $sw_src

# 5. Build
app build -name $app_name

puts "============================================="
puts "  Vitis project created and built!"
puts "  Workspace: $ws_dir"
puts "  App: $app_name"
puts "============================================="
puts ""
puts "To run on Zybo Z7-20:"
puts "  1. Connect JTAG USB"
puts "  2. Open Vitis GUI or use xsct:"
puts "     connect"
puts "     targets -set -filter {name =~ \"ARM*#0\"}"
puts "     rst -system"
puts "     fpga /home/hyeonjun/cnn_pcam_project/cnn_pcam.bit"
puts "     loadhw /home/hyeonjun/cnn_pcam_project/cnn_pcam.xsa"
puts "     source /home/hyeonjun/cnn_pcam_project/vitis_ws/cnn_pcam_platform/ps7_init.tcl"
puts "     ps7_init"
puts "     ps7_post_config"
puts "     dow $ws_dir/$app_name/Debug/$app_name.elf"
puts "     con"
puts ""
puts "  3. Open serial terminal (115200 baud) to see CNN results"
