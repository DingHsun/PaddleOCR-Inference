<script setup>
import { ref, nextTick } from 'vue'

const imageFile = ref(null)
const imageUrl = ref('')
const imgEl = ref(null)
const canvasEl = ref(null)

const detecting = ref(false)
const recognizing = ref(false)
const errorMsg = ref('')
const recognizedText = ref('')
const boxCount = ref(null)

function onFileChange(e) {
  const file = e.target.files[0]
  if (!file) return
  imageFile.value = file
  if (imageUrl.value) URL.revokeObjectURL(imageUrl.value)
  imageUrl.value = URL.createObjectURL(file)
  errorMsg.value = ''
  recognizedText.value = ''
  boxCount.value = null
  clearCanvas()
}

function clearCanvas() {
  const canvas = canvasEl.value
  if (canvas) canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height)
}

async function postImage(path) {
  const res = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/octet-stream' },
    body: imageFile.value,
  })
  const data = await res.json()
  if (!res.ok) throw new Error(data.hint || data.error || `HTTP ${res.status}`)
  return data
}

function drawBoxes(boxes) {
  const img = imgEl.value
  const canvas = canvasEl.value
  canvas.width = img.clientWidth
  canvas.height = img.clientHeight
  const scaleX = img.clientWidth / img.naturalWidth
  const scaleY = img.clientHeight / img.naturalHeight

  const ctx = canvas.getContext('2d')
  ctx.clearRect(0, 0, canvas.width, canvas.height)
  ctx.strokeStyle = '#22c55e'
  ctx.lineWidth = 2

  for (const box of boxes) {
    ctx.beginPath()
    box.forEach(([x, y], i) => {
      const px = x * scaleX
      const py = y * scaleY
      if (i === 0) ctx.moveTo(px, py)
      else ctx.lineTo(px, py)
    })
    ctx.closePath()
    ctx.stroke()
  }
}

async function runDetect() {
  if (!imageFile.value) return
  detecting.value = true
  errorMsg.value = ''
  try {
    const boxes = await postImage('/ocr_detect')
    boxCount.value = boxes.length
    await nextTick()
    drawBoxes(boxes)
  } catch (err) {
    errorMsg.value = err.message
  } finally {
    detecting.value = false
  }
}

async function runRecognize() {
  if (!imageFile.value) return
  recognizing.value = true
  errorMsg.value = ''
  try {
    const data = await postImage('/ocr_recognize')
    recognizedText.value = data.text
  } catch (err) {
    errorMsg.value = err.message
  } finally {
    recognizing.value = false
  }
}
</script>

<template>
  <main>
    <h1>PaddleOCR</h1>

    <input type="file" accept="image/*" @change="onFileChange" />

    <div class="image-wrap" v-show="imageUrl">
      <img ref="imgEl" :src="imageUrl" @load="clearCanvas" />
      <canvas ref="canvasEl" class="overlay"></canvas>
    </div>

    <div class="actions">
      <button :disabled="!imageFile || detecting" @click="runDetect">
        {{ detecting ? 'Detecting...' : 'Detect' }}
      </button>
      <button :disabled="!imageFile || recognizing" @click="runRecognize">
        {{ recognizing ? 'Recognizing...' : 'Recognize' }}
      </button>
    </div>

    <p v-if="boxCount !== null">Found {{ boxCount }} text box(es).</p>
    <p v-if="recognizedText">Recognized text: <strong>{{ recognizedText }}</strong></p>
    <p v-if="errorMsg" class="error">{{ errorMsg }}</p>
  </main>
</template>

<style scoped>
main {
  max-width: 720px;
  margin: 2rem auto;
  padding: 0 1rem;
  font-family: system-ui, sans-serif;
}

.image-wrap {
  position: relative;
  display: inline-block;
  margin-top: 1rem;
  max-width: 100%;
}

.image-wrap img {
  max-width: 100%;
  display: block;
}

.overlay {
  position: absolute;
  top: 0;
  left: 0;
  pointer-events: none;
}

.actions {
  margin-top: 1rem;
  display: flex;
  gap: 0.5rem;
}

.error {
  color: #dc2626;
}
</style>
