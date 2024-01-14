import {
  Layout,
  LayoutProps,
  Line,
  Node,
  NodeProps,
  Rect,
  Shape,
  ShapeProps,
  colorSignal,
  initial,
  signal,
} from "@motion-canvas/2d"
import {
  ColorSignal,
  PossibleColor,
  SignalValue,
  SimpleSignal,
  createRef,
  debug,
  range,
} from "@motion-canvas/core"

export interface SurfaceProps extends ShapeProps {
  albedo?: SignalValue<PossibleColor>
  dashes?: SignalValue<number>
  dashLength?: SignalValue<number>
}

export class Surface extends Shape {
  @initial("#FFF")
  @colorSignal()
  public declare readonly albedo: ColorSignal<this>

  @initial(8)
  @signal()
  public declare readonly dashes: SimpleSignal<number, this>

  @initial(50)
  @signal()
  public declare readonly dashLength: SimpleSignal<number, this>

  private readonly container = createRef<Rect>()

  public constructor(props?: SurfaceProps) {
    super({
      ...props,
    })

    // const N = this.dashes()
    const line_width = this.height() || 16

    this.add(
      <Rect
        ref={this.container}
        fill={this.albedo()}
        width={this.width}
        height={line_width}
        radius={line_width}
      >
        {() =>
          range(Math.floor(this.dashes())).map(i => {
            const dashes = Math.floor(this.dashes())
            const x = () =>
              (i / dashes) * this.container().width() -
              this.container().width() / 2 +
              this.container().width() / dashes / 2 +
              this.dashLength() / 2
            return (
              <Line
                points={[() => [x(), 0], () => [x() - this.dashLength(), this.dashLength()]]}
                lineWidth={line_width}
                stroke={this.albedo()}
                lineCap={"round"}
              />
            )
          })
        }
      </Rect>
    )

    // this.container().add(
    //   range(N).map(i => {
    //     debug(this.container().width() * (i / N))
    //     return (
    //       <Rect
    //         radius={3}
    //         fill={this.albedo()}
    //         x={() => this.container().width() * (i / N)}
    //         y={0}
    //         size={[`${80 / N}%`, 16]}
    //         // topLeft={this.container().topLeft}
    //         rotation={30}
    //       />
    //     )
    //   })
    // )
  }
}
