import { Circle, makeScene2D } from "@motion-canvas/2d"
import { all, createRef, waitFor } from "@motion-canvas/core"
import { Surface } from "../components/surface"

export default makeScene2D(function* (view) {
  // const circle = createRef<Circle>()
  const surface = createRef<Surface>()
  // view.add(<Circle ref={circle} width={200} height={200} fill={"#f1caff"} />)
  view.add(<Surface ref={surface} width={600} albedo={"#fff"} dashes={5} dashLength={50} />)

  // yield* surface().dashLength(50, 0).to(40, 1)
  yield* surface().dashes(1, 0).to(8, 1)

  // yield* waitFor(5)
  // yield* all(
  //   circle().x(300, 0).to(-300, 1).to(300, 1),
  //   circle().fill("#f1caff", 0).to("#A571F4", 1).to("#f1caff", 1)
  // )
})
