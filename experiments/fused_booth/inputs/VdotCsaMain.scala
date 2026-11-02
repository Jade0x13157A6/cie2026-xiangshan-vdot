package top

import xiangshan._
import xiangshan.backend.fu.FuConfig
import xiangshan.backend.fu.wrapper.VIAluFix

object VdotCsaMain {
  def main(args: Array[String]): Unit = {

    val (config, firrtlOpts, firtoolOpts) = ArgParser.parse(
      args ++ Array(
        "--disable-always-basic-diff",
        "--dump-fir",
        "--fpga-platform",
        "--target", "verilog"
      )
    )

    val moduleConfig = config.alterPartial({
      case XSCoreParamsKey => config(XSTileKey).head
    })

    Generator.execute(
      firrtlOpts ++ Array(
        "--full-stacktrace",
        "--target-dir", "build/vdot-csa"
      ),
      new VIAluFix(FuConfig.VialuCfg)(moduleConfig),
      firtoolOpts
    )
  }
}
