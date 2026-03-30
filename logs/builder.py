# 2026-03-30T20:58:18.953908
import vitis

client = vitis.create_client()
client.set_workspace(path="/home/hyeonjun/imx219_pcam_project")

platform = client.create_platform_component(name = "imx219_zybo_platform",hw = "/home/hyeonjun/imx219_pcam_project/cnn_pcam.xsa",os = "standalone",cpu = "ps7_cortexa9_0")

comp = client.create_app_component(name="imx219_cam",platform = "/home/hyeonjun/imx219_pcam_project/imx219_zybo_platform/export/imx219_zybo_platform/imx219_zybo_platform.xpfm",domain = "standalone_ps7_cortexa9_0")

platform = client.get_platform_component(name="imx219_zybo_platform")
status = platform.build()

comp = client.get_component(name="imx219_cam")
comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

comp.build()

