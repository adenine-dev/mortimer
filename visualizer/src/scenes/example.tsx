import { Circle, makeScene2D } from "@motion-canvas/2d"
import { all, createRef, waitFor } from "@motion-canvas/core"

export default makeScene2D(function* (view) {
  const circle = createRef<Circle>()
  view.add(<Circle ref={circle} width={200} height={200} fill={"#f1caff"} />)

  yield* all(
    circle().x(300, 0).to(-300, 1).to(300, 1),
    circle().fill("#f1caff", 0).to("#A571F4", 1).to("#f1caff", 1)
  )
})
