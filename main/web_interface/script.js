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

let isDragging = false;
let currentTool = "painter"; // "painter" or "eraser"
let hasChanged = false; // flag to record if a change occurred during drag

// History management for undo/redo: store snapshots of the grid state as 2D arrays.
const history = [];
let historyIndex = -1;

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
});
document.getElementById('eraserTool').addEventListener('click', () => {
  currentTool = "eraser";
  document.getElementById('eraserTool').classList.add('selected');
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
    innerDiv.style.backgroundColor = colorPicker.value;
  } else if (currentTool === "eraser") {
    innerDiv.style.backgroundColor = "";
  }
}

function clearMatrix() {
  document.querySelectorAll('.matrix .cell div').forEach(inner => {
    inner.style.backgroundColor = '';
  });
  pushHistory();
}

function getMatrixDesign() {
  const rgba2hex = (rgba) => `#${rgba.match(/^rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*(\d+\.{0,1}\d*))?\)$/).slice(1).map((n, i) => (i === 3 ? Math.round(parseFloat(n) * 255) : parseFloat(n)).toString(16).padStart(2, '0').replace('NaN', '')).join('')}`
  const design = [];
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const cell = document.querySelector(`.cell[data-row="${row}"][data-col="${col}"] div`);
      const color = rgba2hex(cell.style.backgroundColor || "rgb(0,0,0)");
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
      const color = cell.style.backgroundColor || "#000000";
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

function onInteractionEnd() {
  if (hasChanged) {
    pushHistory();
    hasChanged = false;
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

createMatrix();
