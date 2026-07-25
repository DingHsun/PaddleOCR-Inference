<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import {
  scalePolygon,
  offsetPolygon,
  boundingBox,
  pointInPolygon,
  normalizeRect,
  cropToBlob,
} from './lib/roi'

const imgEl = ref(null)
const canvasEl = ref(null)
const fileInputEl = ref(null)

const imageUrl = ref('')
const naturalSize = ref({ w: 0, h: 0 })
const selection = ref(null) // {x,y,w,h} in natural coords; null until an image is loaded
const redBoxes = ref([]) // [{ id, points: [[x,y],...] }] in natural coords, after Detect
const selectedBoxId = ref(null)

const detecting = ref(false)
const recognizing = ref(false)
const errorMsg = ref('')
const recognizedText = ref(null) // null = nothing run yet; '' = ran but found no text
const detectStatus = ref('')
const dropActive = ref(false)

// Drag/hit-test state -- plain vars, not reactive, only read inside pointer handlers.
let dragStart = null
let dragCurrent = null
let isDragging = false
let displayedBoxes = [] // [{ id, points }] in displayed (CSS px) coords, rebuilt every draw()

function displayScale() {
  const img = imgEl.value
  if (!img || !naturalSize.value.w) return { x: 1, y: 1 }
  return {
    x: img.clientWidth / naturalSize.value.w,
    y: img.clientHeight / naturalSize.value.h,
  }
}

function loadFile(file) {
  if (!file || !file.type.startsWith('image/')) return
  if (imageUrl.value) URL.revokeObjectURL(imageUrl.value)
  imageUrl.value = URL.createObjectURL(file)
  selection.value = null
  redBoxes.value = []
  selectedBoxId.value = null
  detectStatus.value = ''
  recognizedText.value = null
  errorMsg.value = ''
}

function onFileChange(e) {
  loadFile(e.target.files[0])
}

function onDrop(e) {
  dropActive.value = false
  loadFile(e.dataTransfer.files[0])
}

function onImageLoad() {
  const img = imgEl.value
  naturalSize.value = { w: img.naturalWidth, h: img.naturalHeight }
  selection.value = { x: 0, y: 0, w: img.naturalWidth, h: img.naturalHeight }
  redBoxes.value = []
  selectedBoxId.value = null
  detectStatus.value = ''
  draw()
}

function draw() {
  const canvas = canvasEl.value
  const img = imgEl.value
  if (!canvas || !img || !img.clientWidth) return
  canvas.width = img.clientWidth
  canvas.height = img.clientHeight
  const { x: sx, y: sy } = displayScale()
  const ctx = canvas.getContext('2d')
  ctx.clearRect(0, 0, canvas.width, canvas.height)

  // Green selection box (or the box currently being dragged out).
  const rect = isDragging && dragCurrent ? dragCurrent : selection.value
  if (rect) {
    const dx = rect.x * sx
    const dy = rect.y * sy
    const dw = rect.w * sx
    const dh = rect.h * sy
    ctx.fillStyle = 'rgba(34, 197, 94, 0.12)'
    ctx.fillRect(dx, dy, dw, dh)
    ctx.strokeStyle = '#22c55e'
    ctx.lineWidth = 2
    ctx.setLineDash(isDragging ? [6, 4] : [])
    ctx.strokeRect(dx, dy, dw, dh)
    ctx.setLineDash([])
  }

  // Detect-result boxes: red, or amber if currently selected.
  displayedBoxes = []
  for (const box of redBoxes.value) {
    const displayed = scalePolygon(box.points, sx, sy)
    const path = new Path2D()
    displayed.forEach(([x, y], i) => (i === 0 ? path.moveTo(x, y) : path.lineTo(x, y)))
    path.closePath()
    const selected = box.id === selectedBoxId.value
    ctx.fillStyle = selected ? 'rgba(245, 158, 11, 0.25)' : 'rgba(239, 68, 68, 0.12)'
    ctx.fill(path)
    ctx.strokeStyle = selected ? '#f59e0b' : '#ef4444'
    ctx.lineWidth = selected ? 3 : 2
    ctx.stroke(path)
    displayedBoxes.push({ id: box.id, points: displayed })
  }
}

function canvasPoint(e) {
  const rect = canvasEl.value.getBoundingClientRect()
  return [e.clientX - rect.left, e.clientY - rect.top]
}

function onPointerDown(e) {
  if (!selection.value) return
  dragStart = canvasPoint(e)
  dragCurrent = null
  isDragging = false
  canvasEl.value.setPointerCapture(e.pointerId)
}

function onPointerMove(e) {
  if (!dragStart) return
  const [x, y] = canvasPoint(e)
  const moved = Math.hypot(x - dragStart[0], y - dragStart[1])
  if (!isDragging && moved < 5) return
  isDragging = true
  const { x: sx, y: sy } = displayScale()
  dragCurrent = normalizeRect(
    {
      x: dragStart[0] / sx,
      y: dragStart[1] / sy,
      w: (x - dragStart[0]) / sx,
      h: (y - dragStart[1]) / sy,
    },
    naturalSize.value.w,
    naturalSize.value.h
  )
  draw()
}

function onPointerUp(e) {
  if (!dragStart) return
  const [x, y] = canvasPoint(e)

  if (isDragging) {
    if (dragCurrent && dragCurrent.w > 2 && dragCurrent.h > 2) {
      selection.value = dragCurrent
      redBoxes.value = []
      selectedBoxId.value = null
      detectStatus.value = ''
    }
  } else {
    const hit = displayedBoxes.find((b) => pointInPolygon([x, y], b.points))
    if (hit) {
      selectedBoxId.value = hit.id
      const box = redBoxes.value.find((b) => b.id === hit.id)
      if (box) recognizeBox(box)
    }
  }

  dragStart = null
  dragCurrent = null
  isDragging = false
  draw()
}

async function postBlob(path, blob) {
  const res = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'image/png' },
    body: blob,
  })
  const data = await res.json()
  if (!res.ok) throw new Error(data.hint || data.error || `HTTP ${res.status}`)
  return data
}

async function runDetect() {
  if (!selection.value || !imgEl.value) return
  detecting.value = true
  errorMsg.value = ''
  redBoxes.value = []
  selectedBoxId.value = null
  detectStatus.value = ''
  try {
    const blob = await cropToBlob(imgEl.value, selection.value)
    const boxes = await postBlob('/ocr_detect', blob)
    redBoxes.value = boxes.map((points, i) => ({
      id: i,
      points: offsetPolygon(points, selection.value.x, selection.value.y),
    }))
    detectStatus.value =
      boxes.length > 0
        ? `Detected ${boxes.length} object(s) — click one to recognize it.`
        : 'No text detected in the selected region.'
    draw()
  } catch (err) {
    errorMsg.value = err.message
  } finally {
    detecting.value = false
  }
}

async function runRecognize() {
  if (!selection.value || !imgEl.value) return
  recognizing.value = true
  errorMsg.value = ''
  try {
    const blob = await cropToBlob(imgEl.value, selection.value)
    const data = await postBlob('/ocr_recognize', blob)
    recognizedText.value = data.text
  } catch (err) {
    errorMsg.value = err.message
  } finally {
    recognizing.value = false
  }
}

async function recognizeBox(box) {
  if (!imgEl.value) return
  recognizing.value = true
  errorMsg.value = ''
  try {
    const blob = await cropToBlob(imgEl.value, boundingBox(box.points))
    const data = await postBlob('/ocr_recognize', blob)
    recognizedText.value = data.text
  } catch (err) {
    errorMsg.value = err.message
  } finally {
    recognizing.value = false
  }
}

function resetSelection() {
  if (!naturalSize.value.w) return
  selection.value = { x: 0, y: 0, w: naturalSize.value.w, h: naturalSize.value.h }
  redBoxes.value = []
  selectedBoxId.value = null
  detectStatus.value = ''
  draw()
}

function browseFiles() {
  fileInputEl.value?.click()
}

onMounted(() => window.addEventListener('resize', draw))
onUnmounted(() => window.removeEventListener('resize', draw))
</script>

<template>
  <div class="page">
    <main class="card">
      <header>
        <h1>PaddleOCR</h1>
        <p class="subtitle">Drag a box on the image, then Detect or Recognize.</p>
      </header>

      <input
        ref="fileInputEl"
        type="file"
        accept="image/*"
        class="visually-hidden"
        @change="onFileChange"
      />

      <div
        class="stage"
        :class="{ 'drop-active': dropActive }"
        @dragover.prevent="dropActive = true"
        @dragleave.prevent="dropActive = false"
        @drop.prevent="onDrop"
      >
        <template v-if="imageUrl">
          <img ref="imgEl" :src="imageUrl" @load="onImageLoad" />
          <canvas
            ref="canvasEl"
            class="overlay"
            @pointerdown="onPointerDown"
            @pointermove="onPointerMove"
            @pointerup="onPointerUp"
          ></canvas>
        </template>
        <button v-else type="button" class="dropzone-empty" @click="browseFiles">
          <span class="dropzone-icon">🖼️</span>
          <span>Click to choose an image, or drag one here</span>
        </button>
      </div>

      <div class="actions">
        <button type="button" class="btn secondary" @click="browseFiles">Choose Image</button>
        <button
          type="button"
          class="btn primary"
          :disabled="!selection || detecting"
          @click="runDetect"
        >
          <span v-if="detecting" class="spinner"></span>
          {{ detecting ? 'Detecting…' : 'Detect' }}
        </button>
        <button
          type="button"
          class="btn primary outline"
          :disabled="!selection || recognizing"
          @click="runRecognize"
        >
          <span v-if="recognizing" class="spinner"></span>
          {{ recognizing ? 'Recognizing…' : 'Recognize' }}
        </button>
        <button type="button" class="btn link" :disabled="!selection" @click="resetSelection">
          Reset selection
        </button>
      </div>

      <p v-if="detectStatus" class="status">{{ detectStatus }}</p>

      <div v-if="recognizedText !== null || errorMsg" class="results">
        <div v-if="recognizedText !== null" class="result-box" :class="{ warn: !recognizedText }">
          <span class="result-label">Recognized text</span>
          <p v-if="recognizedText" class="result-text">{{ recognizedText }}</p>
          <p v-else class="result-text warn-text">⚠ No text recognized in the selected region.</p>
        </div>
        <div v-if="errorMsg" class="error-box">{{ errorMsg }}</div>
      </div>
    </main>
  </div>
</template>

<style scoped>
.page {
  min-height: 100vh;
  display: flex;
  justify-content: center;
  padding: 2.5rem 1rem;
  box-sizing: border-box;
}

.card {
  width: 100%;
  max-width: 720px;
  background: var(--card-bg);
  border: 1px solid var(--border);
  border-radius: 16px;
  box-shadow: var(--shadow);
  padding: 2rem;
  box-sizing: border-box;
}

header {
  margin-bottom: 1.5rem;
}

h1 {
  margin: 0 0 0.25rem;
  font-size: 1.75rem;
}

.subtitle {
  margin: 0;
  color: var(--text-muted);
  font-size: 0.95rem;
}

.visually-hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
}

.stage {
  position: relative;
  border: 1px solid var(--border);
  border-radius: 12px;
  overflow: hidden;
  background:
    repeating-conic-gradient(var(--border) 0% 25%, transparent 0% 50%) 50% / 20px 20px;
  min-height: 220px;
  display: flex;
}

.stage.drop-active {
  outline: 2px dashed var(--accent);
  outline-offset: -2px;
}

.stage img {
  display: block;
  width: 100%;
  height: auto;
}

.overlay {
  position: absolute;
  top: 0;
  left: 0;
  touch-action: none;
  cursor: crosshair;
}

.dropzone-empty {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
  padding: 3rem 1rem;
  border: none;
  background: transparent;
  color: var(--text-muted);
  cursor: pointer;
  font: inherit;
}

.dropzone-icon {
  font-size: 2rem;
}

.actions {
  margin-top: 1.25rem;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  align-items: center;
}

.btn {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;
  border-radius: 8px;
  border: 1px solid transparent;
  padding: 0.55rem 1rem;
  font: inherit;
  font-weight: 600;
  cursor: pointer;
  transition: opacity 0.15s ease, transform 0.05s ease;
}

.btn:active:not(:disabled) {
  transform: scale(0.98);
}

.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn.primary {
  background: var(--accent);
  color: var(--accent-contrast);
}

.btn.primary.outline {
  background: transparent;
  color: var(--accent);
  border-color: var(--accent);
}

.btn.secondary {
  background: transparent;
  color: var(--text);
  border-color: var(--border);
}

.btn.link {
  background: transparent;
  color: var(--text-muted);
  padding: 0.55rem 0.25rem;
  margin-left: auto;
  font-weight: 500;
}

.spinner {
  width: 0.9em;
  height: 0.9em;
  border: 2px solid currentColor;
  border-right-color: transparent;
  border-radius: 50%;
  animation: spin 0.6s linear infinite;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

.status {
  margin: 1rem 0 0;
  font-size: 0.9rem;
  color: var(--text-muted);
}

.results {
  margin-top: 1rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.result-box {
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 0.75rem 1rem;
}

.result-label {
  display: block;
  font-size: 0.8rem;
  font-weight: 600;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  margin-bottom: 0.35rem;
}

.result-text {
  margin: 0;
  font-size: 1.05rem;
  word-break: break-word;
}

.result-box.warn {
  border-color: #fcd34d;
  background: rgba(245, 158, 11, 0.08);
}

.warn-text {
  color: #b45309;
}

@media (prefers-color-scheme: dark) {
  .result-box.warn {
    border-color: #78350f;
    background: rgba(245, 158, 11, 0.12);
  }
  .warn-text {
    color: #fcd34d;
  }
}

.error-box {
  border: 1px solid #fca5a5;
  background: rgba(239, 68, 68, 0.08);
  color: #b91c1c;
  border-radius: 10px;
  padding: 0.75rem 1rem;
  font-size: 0.9rem;
}

@media (prefers-color-scheme: dark) {
  .error-box {
    border-color: #7f1d1d;
    background: rgba(239, 68, 68, 0.12);
    color: #fca5a5;
  }
}
</style>
