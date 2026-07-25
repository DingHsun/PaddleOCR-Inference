// Pure geometry helpers for the region-selection / crop workflow.
// Rectangles are {x, y, w, h}; boxes/polygons are arrays of [x, y] points.
// No Vue/DOM state lives here except cropToBlob, which needs a loaded
// <img> element to draw from.

export function scalePoint([x, y], scaleX, scaleY) {
  return [x * scaleX, y * scaleY]
}

export function scaleRect(rect, scaleX, scaleY) {
  return {
    x: rect.x * scaleX,
    y: rect.y * scaleY,
    w: rect.w * scaleX,
    h: rect.h * scaleY,
  }
}

export function scalePolygon(points, scaleX, scaleY) {
  return points.map((p) => scalePoint(p, scaleX, scaleY))
}

// Shifts every point in a polygon by (dx, dy) -- used to map detect-result
// boxes (relative to a cropped region) back into the original image's
// natural coordinate space.
export function offsetPolygon(points, dx, dy) {
  return points.map(([x, y]) => [x + dx, y + dy])
}

export function boundingBox(points) {
  const xs = points.map((p) => p[0])
  const ys = points.map((p) => p[1])
  const x = Math.min(...xs)
  const y = Math.min(...ys)
  return { x, y, w: Math.max(...xs) - x, h: Math.max(...ys) - y }
}

// Ray-casting point-in-polygon test.
export function pointInPolygon([px, py], polygon) {
  let inside = false
  for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
    const [xi, yi] = polygon[i]
    const [xj, yj] = polygon[j]
    const intersects =
      yi > py !== yj > py && px < ((xj - xi) * (py - yi)) / (yj - yi) + xi
    if (intersects) inside = !inside
  }
  return inside
}

// Clamps a rect to stay fully inside [0,0,maxW,maxH], normalizing negative
// width/height that comes from dragging up/left instead of down/right.
export function normalizeRect(rect, maxW, maxH) {
  let { x, y, w, h } = rect
  if (w < 0) {
    x += w
    w = -w
  }
  if (h < 0) {
    y += h
    h = -h
  }
  x = Math.max(0, Math.min(x, maxW))
  y = Math.max(0, Math.min(y, maxH))
  w = Math.max(0, Math.min(w, maxW - x))
  h = Math.max(0, Math.min(h, maxH - y))
  return { x, y, w, h }
}

// Draws the given natural-coordinate rect from `imgEl` onto an offscreen
// canvas and resolves a PNG Blob of just that region.
export function cropToBlob(imgEl, rect) {
  const canvas = document.createElement('canvas')
  canvas.width = Math.max(1, Math.round(rect.w))
  canvas.height = Math.max(1, Math.round(rect.h))
  const ctx = canvas.getContext('2d')
  ctx.drawImage(imgEl, rect.x, rect.y, rect.w, rect.h, 0, 0, canvas.width, canvas.height)
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob) resolve(blob)
      else reject(new Error('crop failed'))
    }, 'image/png')
  })
}
