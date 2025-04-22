const MATRIX_SIZE = 16;
const matrixEl = document.getElementById('matrix');
const colorPicker = document.getElementById('colorPicker');
const clearBtn = document.getElementById('clearBtn');
const exportBtn = document.getElementById('exportBtn');
const applyBtn = document.getElementById('applyBtn');
const themeToggleBtn = document.getElementById('themeToggleBtn');
const undoBtn = document.getElementById('undoBtn');
const redoBtn = document.getElementById('redoBtn');
const imageInput = document.getElementById('imageInput');

// Animation related elements
// Timeline control buttons
const deleteFrameBtn = document.getElementById("deleteFrameBtn");
const insertFrameBtn = document.getElementById("insertFrameBtn");
const moveLeftBtn = document.getElementById("moveLeftBtn");
const moveRightBtn = document.getElementById("moveRightBtn");

const intervalInput = document.getElementById("intervalInput");
const applyAnimationBtn = document.getElementById("applyAnimationBtn");

let isDragging = false;
let currentTool = "painter"; // "painter" or "eraser" or "bucket"
let hasChanged = false; // flag to record if a change occurred during drag

// History management for undo/redo: store snapshots of the grid state as 2D arrays.
const history = [];
let historyIndex = -1;

// Animation storage
let animationFrames = [];
let selectedFrameIndex = 0; // Currently selected frame index in the timeline.

// Flood-fill
function bucketFill(x, y, targetColor, previewHex, scaledHex) {
  const visited = new Set();
  const queue = [[x, y]];

  while (queue.length > 0) {
    const [cx, cy] = queue.pop();
    const key = `${cx},${cy}`;
    if (visited.has(key)) continue;
    visited.add(key);

    const div = document.querySelector(`.cell[data-row='${cy}'][data-col='${cx}'] div`);
    if (!div) continue;

    const divColor = div.dataset.actualColor || "";
    if (divColor !== targetColor) continue;

    div.style.backgroundColor = previewHex;
    div.dataset.actualColor = scaledHex;

    queue.push([cx + 1, cy]);
    queue.push([cx - 1, cy]);
    queue.push([cx, cy + 1]);
    queue.push([cx, cy - 1]);
  }
}

function captureState() {
  const state = [];
  for (let row = 0; row < MATRIX_SIZE; row++) {
    const rowColors = [];
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const cell = document.querySelector(`.cell[data-row="${row}"][data-col="${col}"] div`);
      rowColors.push(cell.style.backgroundColor || "");
    }
    state.push(rowColors);
  }
  return state;
}

function applyState(state) {
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const cell = document.querySelector(`.cell[data-row="${row}"][data-col="${col}"] div`);
      cell.style.backgroundColor = state[row][col];
    }
  }
  // update animation frames
  animationFrames[selectedFrameIndex] = captureState();
  renderFrameTimeline();
}

function pushHistory() {
  // Discard any redo states.
  history.splice(historyIndex + 1);
  history.push(captureState());
  historyIndex = history.length - 1;
}

function undo() {
  if (historyIndex > 0) {
    historyIndex--;
    applyState(history[historyIndex]);
  }
}

function redo() {
  if (historyIndex < history.length - 1) {
    historyIndex++;
    applyState(history[historyIndex]);
  }
}

// Tool selector using placeholder icons.
document.getElementById('painterTool').addEventListener('click', () => {
  currentTool = "painter";
  document.getElementById('painterTool').classList.add('selected');
  document.getElementById('eraserTool').classList.remove('selected');
  document.getElementById('bucketTool').classList.remove('selected');
});
document.getElementById('eraserTool').addEventListener('click', () => {
  currentTool = "eraser";
  document.getElementById('eraserTool').classList.add('selected');
  document.getElementById('painterTool').classList.remove('selected');
  document.getElementById('bucketTool').classList.remove('selected');
});
document.getElementById("bucketTool").addEventListener("click", () => {
  currentTool = "bucket";
  document.getElementById('bucketTool').classList.add('selected');
  document.getElementById('eraserTool').classList.remove('selected');
  document.getElementById('painterTool').classList.remove('selected');
});

// Toggle dark theme.
themeToggleBtn.addEventListener('click', () => {
  document.body.classList.toggle('dark');
});

// Create the grid.
function createMatrix() {
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const cell = document.createElement('div');
      cell.classList.add('cell');
      cell.dataset.row = row;
      cell.dataset.col = col;
      const inner = document.createElement('div');
      inner.dataset.x = col;
      inner.dataset.y = row;
      cell.appendChild(inner);
      
      // Mouse events.
      cell.addEventListener('mousedown', () => {
        isDragging = true;
        hasChanged = true;
        applyTool(inner);
      });
      cell.addEventListener('mouseover', () => {
        if (isDragging) {
          hasChanged = true;
          applyTool(inner);
        }
      });
      // Touch events.
      cell.addEventListener('touchstart', (e) => {
        isDragging = true;
        hasChanged = true;
        applyTool(inner);
        e.preventDefault();
      }, {passive: false});
      cell.addEventListener('touchmove', (e) => {
        e.preventDefault();
        const touch = e.touches[0];
        const target = document.elementFromPoint(touch.clientX, touch.clientY);
        if (target && target.closest('.cell')) {
          hasChanged = true;
          const innerDiv = target.closest('.cell').querySelector('div');
          applyTool(innerDiv);
        }
      }, {passive: false});
      
      matrixEl.appendChild(cell);
    }
  }
  pushHistory();
}

function applyTool(innerDiv) {
  if (currentTool === "painter") {
    const originalHex = colorPicker.value; 
    const scaledHex = scaleColorForLED(originalHex);
    const previewHex = originalHex;
    innerDiv.style.backgroundColor = previewHex;
    innerDiv.dataset.actualColor = scaledHex;
  } else if (currentTool === "eraser") {
    innerDiv.style.backgroundColor = "";
    delete innerDiv.dataset.actualColor;
  } else if (currentTool === "bucket") {
    const originalHex = colorPicker.value;
    const scaledHex = scaleColorForLED(originalHex);
    const previewHex = originalHex;

    const targetColor = innerDiv.dataset.actualColor || "";
    const x = parseInt(innerDiv.dataset.x);
    const y = parseInt(innerDiv.dataset.y);

    bucketFill(x, y, targetColor, previewHex, scaledHex);
  }
}

function clearMatrix() {
  document.querySelectorAll('.matrix .cell div').forEach(inner => {
    inner.style.backgroundColor = '';
    delete inner.dataset.actualColor;
  });
  pushHistory();
  // update animation frames
  animationFrames[selectedFrameIndex] = captureState();
  renderFrameTimeline();
}

function scaleColorForLED(hex) {
  // Remove the "#" and parse channels as base-16 numbers.
  let r = parseInt(hex.substring(1, 3), 16);
  let g = parseInt(hex.substring(3, 5), 16);
  let b = parseInt(hex.substring(5, 7), 16);

  // Scale: factor = 50/255
  const factor = 50 / 255;
  r = Math.round(r * factor);
  g = Math.round(g * factor);
  b = Math.round(b * factor);

  return `rgb(${r}, ${g}, ${b})`;
}

const rgba2hex = (rgba) => `#${rgba.match(/^rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*(\d+\.{0,1}\d*))?\)$/).slice(1).map((n, i) => (i === 3 ? Math.round(parseFloat(n) * 255) : parseFloat(n)).toString(16).padStart(2, '0').replace('NaN', '')).join('')}`

function getMatrixDesign() {
  const design = [];
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const cell = document.querySelector(`.cell[data-row="${row}"][data-col="${col}"] div`);
      const color = rgba2hex(cell.dataset.actualColor || "rgb(0,0,0)");
      design.push(color);
    }
  }
  return design;
}

function transformImageToLEDMatrix(image) {
  // Create an offscreen canvas with the desired LED matrix dimensions.
  const canvas = document.createElement('canvas');
  canvas.width = MATRIX_SIZE;
  canvas.height = MATRIX_SIZE;
  const ctx = canvas.getContext('2d');

  // Draw the image scaled into the 16x16 canvas.
  // This will automatically scale the image.
  ctx.drawImage(image, 0, 0, MATRIX_SIZE, MATRIX_SIZE);

  // Extract pixel data.
  const imageData = ctx.getImageData(0, 0, MATRIX_SIZE, MATRIX_SIZE);
  const data = imageData.data; // Array of RGBA values.

  // Loop through each pixel.
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const index = (row * MATRIX_SIZE + col) * 4; // 4 channels per pixel.
      const r = data[index];
      const g = data[index + 1];
      const b = data[index + 2];
      // Convert to hex string.
      const hex =
        '#' +
        [r, g, b]
          .map((v) => v.toString(16).padStart(2, '0'))
          .join('');

      // Update the preview grid cell.
      const cell = document.querySelector(
        `.cell[data-row="${row}"][data-col="${col}"] div`
      );
      if (cell) {
        cell.style.backgroundColor = hex;
        cell.dataset.actualColor = scaleColorForLED(hex);
      }
    }
  }
}

function importImage(event) {
  const file = event.target.files[0];
  if (!file) return;
  
  const reader = new FileReader();
  reader.onload = function(e) {
    const img = new Image();
    img.onload = () => {
      transformImageToLEDMatrix(img);
    };
    img.src = e.target.result;
  };
  reader.readAsDataURL(file);
}

function exportAsPNG() {
  const canvasSize = 512;
  const cellSize = canvasSize / MATRIX_SIZE;
  const canvas = document.createElement('canvas');
  canvas.width = canvasSize;
  canvas.height = canvasSize;
  const ctx = canvas.getContext('2d');
  
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const cell = document.querySelector(`.cell[data-row="${row}"][data-col="${col}"] div`);
      const color = cell.dataset.actualColor || "#000000";
      ctx.fillStyle = color;
      ctx.fillRect(col * cellSize, row * cellSize, cellSize, cellSize);
    }
  }
  canvas.toBlob((blob) => {
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'framepix.png';
    a.click();
    URL.revokeObjectURL(url);
  });
}

function applyDesign() {
  const design = getMatrixDesign();
  console.log("Design:", design);
  fetch('/matrix', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ matrix: design })
  })
  .then(res => res.ok ? console.log("Matrix sent!") : console.error("Send failed"))
  .catch(console.error);
}

function applyAnimation() {
  // Update the current frame with the latest grid state
  animationFrames[selectedFrameIndex] = captureState();

  // Convert a 2D state into a 1D LED-scaled hex array
  function processFrame(frame2D) {
    const frameFlat = [];
    for (let row = 0; row < MATRIX_SIZE; row++) {
      for (let col = 0; col < MATRIX_SIZE; col++) {
        const rgba = frame2D[row][col] || "rgb(0, 0, 0)";
        const hex = rgba2hex(rgba);
        const scaled = rgba2hex(scaleColorForLED(hex));
        frameFlat.push(scaled);
      }
    }
    return frameFlat;
  }

  // Process all frames into 1D LED-scaled arrays
  const processedFrames = animationFrames.map(processFrame);

  // Prepare the payload
  const payload = {
    interval_ms: parseInt(intervalInput.value),
    frames: processedFrames
  };

  console.log("Animation:", payload);

  fetch('/animation', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  })
  .then(res => res.ok ? console.log("Animation sent!") : console.error("Animation send failed"))
  .catch(console.error);
}

function onInteractionEnd() {
  if (hasChanged) {
    pushHistory();
    hasChanged = false;
    // Update the animations related stuff
    animationFrames[selectedFrameIndex] = captureState();
    renderFrameTimeline();
  }
  isDragging = false;
}

document.addEventListener('mouseup', onInteractionEnd);
document.addEventListener('touchend', onInteractionEnd);

undoBtn.addEventListener('click', undo);
redoBtn.addEventListener('click', redo);
clearBtn.addEventListener('click', clearMatrix);
imageInput.addEventListener('change', importImage);
exportBtn.addEventListener('click', exportAsPNG);
applyBtn.addEventListener('click', applyDesign);

// Animation Timeline Functions
function renderFrameTimeline() {
  const timeline = document.getElementById("frameTimeline");
  timeline.innerHTML = "";
  animationFrames.forEach((frame, index) => {
    const thumb = document.createElement("div");
    thumb.classList.add("frame-thumb");
    thumb.dataset.frameIndex = index;
    if (index === selectedFrameIndex) {
      thumb.classList.add("selected-frame");
    }
    
    const numOverlay = document.createElement("div");
    numOverlay.classList.add("frame-number");
    numOverlay.textContent = index + 1;
    thumb.appendChild(numOverlay);
    
    const canvas = document.createElement("canvas");
    canvas.width = 64;
    canvas.height = 64;
    const ctx = canvas.getContext("2d");
    const cellSize = canvas.width / MATRIX_SIZE;
    for (let row = 0; row < MATRIX_SIZE; row++) {
      for (let col = 0; col < MATRIX_SIZE; col++) {
        const hex = frame[row][col] || "#000000";
        ctx.fillStyle = hex;
        ctx.fillRect(col * cellSize, row * cellSize, cellSize, cellSize);
      }
    }
    thumb.appendChild(canvas);
    
    thumb.addEventListener("click", () => {
      selectedFrameIndex = index;
      applyState(animationFrames[index]);
      renderFrameTimeline();
    });
    
    timeline.appendChild(thumb);
  });
}

// Animation timeline Control Functions
deleteFrameBtn.addEventListener("click", () => {
  if (animationFrames.length > 1) {
    animationFrames.splice(selectedFrameIndex, 1);
    if (selectedFrameIndex >= animationFrames.length) {
      selectedFrameIndex = animationFrames.length - 1;
    }
    applyState(animationFrames[selectedFrameIndex]);
    renderFrameTimeline();
  } else {
    alert("Cannot delete the only frame.");
  }
});

insertFrameBtn.addEventListener("click", () => {
  const newFrame = JSON.parse(JSON.stringify(animationFrames[selectedFrameIndex]));
  animationFrames.splice(selectedFrameIndex + 1, 0, newFrame);
  selectedFrameIndex++;
  renderFrameTimeline();
});

moveLeftBtn.addEventListener("click", () => {
  if (selectedFrameIndex > 0) {
    [animationFrames[selectedFrameIndex - 1], animationFrames[selectedFrameIndex]] =
      [animationFrames[selectedFrameIndex], animationFrames[selectedFrameIndex - 1]];
    selectedFrameIndex--;
    renderFrameTimeline();
  }
});

moveRightBtn.addEventListener("click", () => {
  if (selectedFrameIndex < animationFrames.length - 1) {
    [animationFrames[selectedFrameIndex + 1], animationFrames[selectedFrameIndex]] =
      [animationFrames[selectedFrameIndex], animationFrames[selectedFrameIndex + 1]];
    selectedFrameIndex++;
    renderFrameTimeline();
  }
});

applyAnimationBtn.addEventListener("click", applyAnimation);

// Initialization
createMatrix();

// For animations, initialize with at least one frame from the current state.
animationFrames = [captureState()];
renderFrameTimeline();
