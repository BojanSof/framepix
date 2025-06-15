const MATRIX_SIZE = 16;
const wifiChangeBtn = document.getElementById('wifiChangeBtn');
const matrixEl = document.getElementById('matrix');
const colorPicker = document.getElementById('colorPicker');
const clearBtn = document.getElementById('clearBtn');
const themeToggleBtn = document.getElementById('themeToggleBtn');
const undoBtn = document.getElementById('undoBtn');
const redoBtn = document.getElementById('redoBtn');
const importImageBtn = document.getElementById('importImage');
const exportImageBtn = document.getElementById('exportImageBtn');
const applyBtn = document.getElementById('applyImageBtn');

// Animation related elements
// Timeline control buttons
const deleteFrameBtn = document.getElementById("deleteFrameBtn");
const insertFrameBtn = document.getElementById("insertFrameBtn");
const moveLeftBtn = document.getElementById("moveLeftBtn");
const moveRightBtn = document.getElementById("moveRightBtn");

const intervalInput = document.getElementById("intervalInput");
const applyAnimationBtn = document.getElementById("applyAnimationBtn");

const importFramesBtn = document.getElementById("importFrames");
const exportFramesBtn = document.getElementById("exportFramesBtn");

let isDragging = false;
let currentTool = "painter"; // "painter" or "eraser" or "bucket"
let hasChanged = false; // flag to record if a change occurred during drag

// History management for undo/redo: store snapshots of the grid state as 2D arrays.
let perFrameHistory = [];
let perFrameHistoryIndex = [];

function initHistoryForFrame(frameIdx) {
  const state = animationFrames[frameIdx];
  perFrameHistory[frameIdx] = [ state.map(row => [...row]) ]; // deep copy 2D
  perFrameHistoryIndex[frameIdx] = 0;
}

// Animation storage
let animationFrames = [];
let selectedFrameIndex = 0; // Currently selected frame index in the timeline.

// Flood-fill
function bucketFill(x, y, targetColor, previewHex) {
  const visited = new Set();
  const queue = [[x, y]];

  while (queue.length > 0) {
    const [cx, cy] = queue.pop();
    const key = `${cx},${cy}`;
    if (visited.has(key)) continue;
    visited.add(key);

    const div = document.querySelector(`.cell[data-row='${cy}'][data-col='${cx}'] div`);
    if (!div) continue;

    const divColor = div.style.backgroundColor || "";
    if (divColor !== targetColor) continue;

    div.style.backgroundColor = previewHex;

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
  const idx = selectedFrameIndex;
  const stack = perFrameHistory[idx];
  // discard any redo states
  stack.splice(perFrameHistoryIndex[idx] + 1);
  stack.push(captureState());
  perFrameHistoryIndex[idx] = stack.length - 1;
}

function undo() {
  const idx = selectedFrameIndex;
  const stack = perFrameHistory[idx];
  let hi = perFrameHistoryIndex[idx];
  if (hi > 0) {
    hi--;
    perFrameHistoryIndex[idx] = hi;
    applyState(stack[hi]);
  }
}

function redo() {
  const idx = selectedFrameIndex;
  const stack = perFrameHistory[idx];
  let hi = perFrameHistoryIndex[idx];
  if (hi < stack.length - 1) {
    hi++;
    perFrameHistoryIndex[idx] = hi;
    applyState(stack[hi]);
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

wifiChangeBtn.addEventListener('click', () => {
  fetch('wifi-change', {
    method: 'POST'
  })
  .then(res => res.ok ? console.log("WiFi change initiated") : console.error("WiFi change failed"))
  .catch(console.error);
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
}

function applyTool(innerDiv) {
  if (currentTool === "painter") {
    const colorHex = colorPicker.value; 
    innerDiv.style.backgroundColor = colorHex;
  } else if (currentTool === "eraser") {
    innerDiv.style.backgroundColor = "";
  } else if (currentTool === "bucket") {
    const colorHex = colorPicker.value;

    const targetColor = innerDiv.style.backgroundColor || "";
    const x = parseInt(innerDiv.dataset.x);
    const y = parseInt(innerDiv.dataset.y);

    bucketFill(x, y, targetColor, colorHex);
  }
}

function clearMatrix() {
  document.querySelectorAll('.matrix .cell div').forEach(inner => {
    inner.style.backgroundColor = '';
  });
  pushHistory();
  // update animation frames
  animationFrames[selectedFrameIndex] = captureState();
  renderFrameTimeline();
}

const rgba2hex = (rgba) => `#${rgba.match(/^rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*(\d+\.{0,1}\d*))?\)$/).slice(1).map((n, i) => (i === 3 ? Math.round(parseFloat(n) * 255) : parseFloat(n)).toString(16).padStart(2, '0').replace('NaN', '')).join('')}`

function getMatrixDesign() {
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
  // Update the animations related stuff
  animationFrames[selectedFrameIndex] = captureState();
  renderFrameTimeline();
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
  event.target.value = ''; // Reset the input value to allow re-uploading the same file
}

const CANVAS_SIZE = 512;

function createCanvasFromMatrix(matrix, canvasSize) {
  const cellSize = canvasSize / MATRIX_SIZE;
  const canvas = document.createElement('canvas');
  canvas.width = canvasSize;
  canvas.height = canvasSize;
  const ctx = canvas.getContext('2d');
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const color = matrix[row * MATRIX_SIZE + col];
      ctx.fillStyle = color;
      ctx.fillRect(col * cellSize, row * cellSize, cellSize, cellSize);
    }
  }
  return canvas;
}

function exportAsPNG() {
  const matrix = getMatrixDesign();
  const canvas = createCanvasFromMatrix(matrix, CANVAS_SIZE);
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
  const design = [];
  for (let row = 0; row < MATRIX_SIZE; row++) {
    for (let col = 0; col < MATRIX_SIZE; col++) {
      const cell = document.querySelector(`.cell[data-row="${row}"][data-col="${col}"] div`);
      const color = rgba2hex(cell.style.backgroundColor || "rgb(0,0,0)");
      design.push(color);
    }
  }
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

  // Convert a 2D state into a 1D hex array
  function processFrame(frame2D) {
    const frameFlat = [];
    for (let row = 0; row < MATRIX_SIZE; row++) {
      for (let col = 0; col < MATRIX_SIZE; col++) {
        const rgba = frame2D[row][col] || "rgb(0, 0, 0)";
        const hex = rgba2hex(rgba);
        frameFlat.push(hex);
      }
    }
    return frameFlat;
  }

  // Process all frames into 1D hex arrays
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
importImageBtn.addEventListener('change', importImage);
exportImageBtn.addEventListener('click', exportAsPNG);
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
    perFrameHistory.splice(selectedFrameIndex, 1);
    perFrameHistoryIndex.splice(selectedFrameIndex, 1);

    if (selectedFrameIndex >= animationFrames.length) {
      selectedFrameIndex = animationFrames.length - 1;
    }
    applyState(animationFrames[selectedFrameIndex]);
    renderFrameTimeline();
  }
});

insertFrameBtn.addEventListener("click", () => {
  const newFrame = JSON.parse(JSON.stringify(animationFrames[selectedFrameIndex]));
  animationFrames.splice(selectedFrameIndex + 1, 0, newFrame);
  // init history for the new frame
  perFrameHistory.splice(selectedFrameIndex + 1, 0, undefined);
  perFrameHistoryIndex.splice(selectedFrameIndex + 1, 0, 0);
  initHistoryForFrame(selectedFrameIndex + 1);

  selectedFrameIndex++;
  renderFrameTimeline();
});

moveLeftBtn.addEventListener("click", () => {
  if (selectedFrameIndex > 0) {
    const i = selectedFrameIndex;
    // swap frames
    [animationFrames[i-1], animationFrames[i]] = [animationFrames[i], animationFrames[i-1]];
    // swap history
    [perFrameHistory[i-1], perFrameHistory[i]] = [perFrameHistory[i], perFrameHistory[i-1]];
    [perFrameHistoryIndex[i-1], perFrameHistoryIndex[i]] = [perFrameHistoryIndex[i], perFrameHistoryIndex[i-1]];
    selectedFrameIndex--;
    renderFrameTimeline();
  }
});

moveRightBtn.addEventListener("click", () => {
  if (selectedFrameIndex < animationFrames.length - 1) {
    const i = selectedFrameIndex;
    [animationFrames[i+1], animationFrames[i]] = [animationFrames[i], animationFrames[i+1]];
    [perFrameHistory[i+1], perFrameHistory[i]] = [perFrameHistory[i], perFrameHistory[i+1]];
    [perFrameHistoryIndex[i+1], perFrameHistoryIndex[i]] = [perFrameHistoryIndex[i], perFrameHistoryIndex[i+1]];
    selectedFrameIndex++;
    renderFrameTimeline();
  }
});

applyAnimationBtn.addEventListener("click", applyAnimation);

importFramesBtn.addEventListener("change", (e) => {
  const files = Array.from(e.target.files);
  const readerPromises = files.map(file => new Promise((resolve) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.src = URL.createObjectURL(file);
  }));

  Promise.all(readerPromises).then(images => {
    animationFrames = images.map(img => {
      const canvas = document.createElement('canvas');
      canvas.width = MATRIX_SIZE;
      canvas.height = MATRIX_SIZE;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(img, 0, 0, MATRIX_SIZE, MATRIX_SIZE);
      const imageData = ctx.getImageData(0, 0, MATRIX_SIZE, MATRIX_SIZE).data;

      const frame = [];
      for (let row = 0; row < MATRIX_SIZE; row++) {
        const rowColors = [];
        for (let col = 0; col < MATRIX_SIZE; col++) {
          const i = (row * MATRIX_SIZE + col) * 4;
          const r = imageData[i];
          const g = imageData[i + 1];
          const b = imageData[i + 2];
          rowColors.push(`rgb(${r}, ${g}, ${b})`);
        }
        frame.push(rowColors);
      }
      return frame;
    });
    // create per frame history for each frame and init
    perFrameHistory = animationFrames.map(frame => [ frame.map(row => [...row]) ]);
    perFrameHistoryIndex = animationFrames.map(() => 0);
    animationFrames.forEach((_, index) => initHistoryForFrame(index));

    selectedFrameIndex = 0;
    applyState(animationFrames[0]);
    renderFrameTimeline();
  });
});

exportFramesBtn.addEventListener('click', async () => {
  const zip = new JSZip();
  const frameInterval = parseInt(intervalInput.value);
  const frameFolder = zip.folder("frames");

  for (let i = 0; i < animationFrames.length; i++) {
    const frame = animationFrames[i];
    const flatFrame = frame.flat().map(color => color || "#000000");
    const canvas = createCanvasFromMatrix(flatFrame, CANVAS_SIZE);
    const blob = await new Promise(resolve => canvas.toBlob(resolve, 'image/png'));
    frameFolder.file(`frame_${String(i).padStart(3, '0')}.png`, blob);
  }

  // Add interval text file
  zip.file("interval.txt", frameInterval.toString());

  // Generate ZIP and trigger download
  const zipBlob = await zip.generateAsync({ type: 'blob' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(zipBlob);
  a.download = "animation.zip";
  a.click();
});

// Gallery functionality
const designsTab = document.getElementById('designsTab');
const animationsTab = document.getElementById('animationsTab');
const designsGallery = document.getElementById('designsGallery');
const animationsGallery = document.getElementById('animationsGallery');
const saveDialog = document.getElementById('saveDialog');
const saveNameInput = document.getElementById('saveNameInput');
const saveDesignButton = document.getElementById('saveDesignButton');
const saveAnimationButton = document.getElementById('saveAnimationButton');

let currentSaveType = null;

designsTab.addEventListener('click', () => {
    designsTab.classList.add('active');
    animationsTab.classList.remove('active');
    designsGallery.classList.add('active');
    animationsGallery.classList.remove('active');
});

animationsTab.addEventListener('click', () => {
    animationsTab.classList.add('active');
    designsTab.classList.remove('active');
    animationsGallery.classList.add('active');
    designsGallery.classList.remove('active');
});

function showSaveDialog(type) {
    currentSaveType = type;
    saveDialog.classList.add('active');
    saveNameInput.value = '';
    saveNameInput.focus();
}

function hideSaveDialog() {
    saveDialog.classList.remove('active');
    currentSaveType = null;
}

saveDesignButton.addEventListener('click', () => showSaveDialog('design'));
saveAnimationButton.addEventListener('click', () => showSaveDialog('animation'));

saveDialog.querySelector('.cancel').addEventListener('click', hideSaveDialog);

saveDialog.querySelector('.save').addEventListener('click', async () => {
    const name = saveNameInput.value.trim();
    if (!name) return;

    try {
        if (currentSaveType === 'design') {
            const pixels = getMatrixDesign();
            const response = await fetch('/save-design', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name, matrix: pixels })
            });
            if (!response.ok) throw new Error('Failed to save design');
        } else if (currentSaveType === 'animation') {
            const frames = animationFrames.map(frame => {
                const framePixels = [];
                for (let row = 0; row < MATRIX_SIZE; row++) {
                    for (let col = 0; col < MATRIX_SIZE; col++) {
                        const rgba = frame[row][col] || "rgb(0, 0, 0)";
                        const hex = rgba2hex(rgba);
                        framePixels.push(hex);
                    }
                }
                return framePixels;
            });
            const response = await fetch('/save-animation', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ 
                    name, 
                    interval_ms: parseInt(intervalInput.value),
                    frames 
                })
            });
            if (!response.ok) throw new Error('Failed to save animation');
        }
        hideSaveDialog();
        loadGallery();
    } catch (error) {
        console.error('Error saving:', error);
        alert('Failed to save. Please try again.');
    }
});

async function loadGallery() {
    try {
        // Get last used item
        const lastUsedResponse = await fetch('/load-last-used');
        const lastUsedData = lastUsedResponse.ok ? await lastUsedResponse.json() : null;

        const designsResponse = await fetch('/list-designs');
        const designsData = await designsResponse.json();
        designsGallery.innerHTML = designsData.designs.map(name => `
            <div class="gallery-item ${lastUsedData && !lastUsedData.isAnimation && lastUsedData.name === name ? 'last-used' : ''}" data-name="${name}">
                <div class="gallery-item-preview">
                    <canvas width="64" height="64" style="width: 100%; height: 100%;"></canvas>
                </div>
                <div class="gallery-item-name">${name}</div>
                <div class="gallery-item-actions">
                    <button onclick="loadDesign('${name}')">Load</button>
                    <button onclick="deleteDesign('${name}')">Delete</button>
                    <button onclick="setLastUsed('${name}', false)" class="set-default-btn">Set as Default</button>
                </div>
            </div>
        `).join('');

        // Load and render design previews
        for (const name of designsData.designs) {
            try {
                const response = await fetch(`/load-design?name=${encodeURIComponent(name)}`);
                const design = await response.json();
                const canvas = designsGallery.querySelector(`[data-name="${name}"] canvas`);
                if (canvas) {
                    const ctx = canvas.getContext('2d');
                    const cellSize = canvas.width / MATRIX_SIZE;
                    for (let row = 0; row < MATRIX_SIZE; row++) {
                        for (let col = 0; col < MATRIX_SIZE; col++) {
                            const color = design.matrix[row * MATRIX_SIZE + col];
                            ctx.fillStyle = color;
                            ctx.fillRect(col * cellSize, row * cellSize, cellSize, cellSize);
                        }
                    }
                }
            } catch (error) {
                console.error(`Error loading design preview for ${name}:`, error);
            }
        }

        const animationsResponse = await fetch('/list-animations');
        const animationsData = await animationsResponse.json();
        animationsGallery.innerHTML = animationsData.animations.map(name => `
            <div class="gallery-item ${lastUsedData && lastUsedData.isAnimation && lastUsedData.name === name ? 'last-used' : ''}" data-name="${name}">
                <div class="gallery-item-preview">
                    <canvas width="64" height="64" style="width: 100%; height: 100%;"></canvas>
                </div>
                <div class="gallery-item-name">${name}</div>
                <div class="gallery-item-actions">
                    <button onclick="loadAnimation('${name}')">Load</button>
                    <button onclick="deleteAnimation('${name}')">Delete</button>
                    <button onclick="setLastUsed('${name}', true)" class="set-default-btn">Set as Default</button>
                </div>
            </div>
        `).join('');

        // Load and render animation previews (first frame only)
        for (const name of animationsData.animations) {
            try {
                const response = await fetch(`/load-animation?name=${encodeURIComponent(name)}`);
                const animation = await response.json();
                const canvas = animationsGallery.querySelector(`[data-name="${name}"] canvas`);
                if (canvas && animation.frames.length > 0) {
                    const ctx = canvas.getContext('2d');
                    const cellSize = canvas.width / MATRIX_SIZE;
                    const firstFrame = animation.frames[0];
                    for (let row = 0; row < MATRIX_SIZE; row++) {
                        for (let col = 0; col < MATRIX_SIZE; col++) {
                            const color = firstFrame[row * MATRIX_SIZE + col];
                            ctx.fillStyle = color;
                            ctx.fillRect(col * cellSize, row * cellSize, cellSize, cellSize);
                        }
                    }
                }
            } catch (error) {
                console.error(`Error loading animation preview for ${name}:`, error);
            }
        }
    } catch (error) {
        console.error('Error loading gallery:', error);
    }
}

async function loadDesign(name) {
    try {
        const response = await fetch(`/load-design?name=${encodeURIComponent(name)}`);
        const design = await response.json();
        
        // Convert 1D array to 2D matrix
        const matrix2D = [];
        for (let row = 0; row < MATRIX_SIZE; row++) {
            const rowColors = [];
            for (let col = 0; col < MATRIX_SIZE; col++) {
                const color = design.matrix[row * MATRIX_SIZE + col];
                const hex = color.startsWith('#') ? color : `#${color}`;
                const r = parseInt(hex.slice(1, 3), 16);
                const g = parseInt(hex.slice(3, 5), 16);
                const b = parseInt(hex.slice(5, 7), 16);
                rowColors.push(`rgb(${r}, ${g}, ${b})`);
            }
            matrix2D.push(rowColors);
        }
        
        applyState(matrix2D);
    } catch (error) {
        console.error('Error loading design:', error);
        alert('Failed to load design. Please try again.');
    }
}

async function loadAnimation(name) {
    try {
        const response = await fetch(`/load-animation?name=${encodeURIComponent(name)}`);
        const animation = await response.json();
        animationFrames = animation.frames.map(frame => {
            const frame2D = [];
            for (let row = 0; row < MATRIX_SIZE; row++) {
                const rowColors = [];
                for (let col = 0; col < MATRIX_SIZE; col++) {
                    const color = frame[row * MATRIX_SIZE + col];
                    const hex = color.startsWith('#') ? color : `#${color}`;
                    const r = parseInt(hex.slice(1, 3), 16);
                    const g = parseInt(hex.slice(3, 5), 16);
                    const b = parseInt(hex.slice(5, 7), 16);
                    rowColors.push(`rgb(${r}, ${g}, ${b})`);
                }
                frame2D.push(rowColors);
            }
            return frame2D;
        });
        intervalInput.value = animation.interval_ms;
        selectedFrameIndex = 0;
        applyState(animationFrames[0]);
        renderFrameTimeline();
    } catch (error) {
        console.error('Error loading animation:', error);
        alert('Failed to load animation. Please try again.');
    }
}

async function deleteDesign(name) {
    if (!confirm(`Are you sure you want to delete "${name}"?`)) return;
    try {
        const response = await fetch(`/delete-design?name=${encodeURIComponent(name)}`, { 
            method: 'DELETE'
        });
        if (!response.ok) throw new Error('Failed to delete design');
        loadGallery();
    } catch (error) {
        console.error('Error deleting design:', error);
        alert('Failed to delete design. Please try again.');
    }
}

async function deleteAnimation(name) {
    if (!confirm(`Are you sure you want to delete "${name}"?`)) return;
    try {
        const response = await fetch(`/delete-animation?name=${encodeURIComponent(name)}`, { 
            method: 'DELETE'
        });
        if (!response.ok) throw new Error('Failed to delete animation');
        loadGallery();
    } catch (error) {
        console.error('Error deleting animation:', error);
        alert('Failed to delete animation. Please try again.');
    }
}

// Add this with other button declarations at the top
const clearStorageBtn = document.getElementById('clearStorageBtn');

// Add this with other event listeners
clearStorageBtn.addEventListener('click', async () => {
    if (!confirm('Are you sure you want to clear all designs and animations? This cannot be undone.')) {
        return;
    }
    
    try {
        const response = await fetch('/clear-storage', {
            method: 'POST'
        });
        if (!response.ok) throw new Error('Failed to clear storage');
        alert('Storage cleared successfully');
        loadGallery(); // Refresh the gallery
    } catch (error) {
        console.error('Error clearing storage:', error);
        alert('Failed to clear storage. Please try again.');
    }
});

// Initialization
createMatrix();

// For animations, initialize with at least one frame from the current state.
animationFrames = [captureState()];
renderFrameTimeline();
initHistoryForFrame(0);

// Load the last used design/animation
async function loadLastUsed() {
    try {
        const response = await fetch('/load-last-used');
        if (!response.ok) return;
        
        const data = await response.json();
        if (data.isAnimation) {
            await loadAnimation(data.name);
        } else {
            await loadDesign(data.name);
        }
    } catch (error) {
        console.error('Error loading last used:', error);
    }
}

loadGallery();
loadLastUsed();

async function setLastUsed(name, isAnimation) {
    try {
        const response = await fetch('/set-last-used', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, isAnimation })
        });
        if (!response.ok) throw new Error('Failed to set last used item');
        loadGallery(); // Refresh gallery to update UI
    } catch (error) {
        console.error('Error setting last used item:', error);
        alert('Failed to set as default. Please try again.');
    }
}
