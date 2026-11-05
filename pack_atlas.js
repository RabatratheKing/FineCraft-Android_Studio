const fs = require('fs');
const path = require('path');
const PNG = require('pngjs').PNG;
function collectImages(dir, fileList) {
    if (!fs.existsSync(dir)) return;
    const files = fs.readdirSync(dir);
    for (const file of files) {
        const fullPath = path.join(dir, file);
        if (fs.statSync(fullPath).isDirectory()) {
            collectImages(fullPath, fileList);
        } else if (file.endsWith('.png')) {
            fileList.push(fullPath);
        }
    }
}
const images = [];
collectImages('app/src/main/assets/Classic_Faithful/assets/minecraft/textures/block', images);
collectImages('app/src/main/assets/Classic_Faithful/assets/minecraft/textures/item', images);
images.sort();
const tileSize = 64;
const gridSize = 64;
const atlasWidth = tileSize * gridSize;
const atlasHeight = tileSize * gridSize;
const atlas = new PNG({ width: atlasWidth, height: atlasHeight });
const mapping = {};
let index = 0;
for (const imgPath of images) {
    if (index >= gridSize * gridSize) break;
    const data = fs.readFileSync(imgPath);
    let img;
    try {
        img = PNG.sync.read(data);
    } catch (e) {
        console.error('Failed to read', imgPath);
        continue;
    }
    const filename = path.basename(imgPath, '.png');
    mapping[filename] = index;
    const tx = index % gridSize;
    const ty = Math.floor(index / gridSize);
    const copyW = Math.min(img.width, tileSize);
    const copyH = Math.min(img.height, tileSize);
    for (let y = 0; y < copyH; y++) {
        for (let x = 0; x < copyW; x++) {
            const srcIdx = (y * img.width + x) * 4;
            const dstIdx = ((ty * tileSize + y) * atlasWidth + (tx * tileSize + x)) * 4;
            atlas.data[dstIdx] = img.data[srcIdx];
            atlas.data[dstIdx + 1] = img.data[srcIdx + 1];
            atlas.data[dstIdx + 2] = img.data[srcIdx + 2];
            atlas.data[dstIdx + 3] = img.data[srcIdx + 3];
        }
    }
    index++;
}

// Generate missing texture at index 4095
const missingIndex = 4095;
mapping['__missing__'] = missingIndex;
const mtx = missingIndex % gridSize;
const mty = Math.floor(missingIndex / gridSize);
for (let y = 0; y < tileSize; y++) {
    for (let x = 0; x < tileSize; x++) {
        const dstIdx = ((mty * tileSize + y) * atlasWidth + (mtx * tileSize + x)) * 4;
        const isMagenta = ((Math.floor(x / 8) + Math.floor(y / 8)) % 2) === 0;
        atlas.data[dstIdx] = isMagenta ? 255 : 0;
        atlas.data[dstIdx + 1] = 0;
        atlas.data[dstIdx + 2] = isMagenta ? 255 : 0;
        atlas.data[dstIdx + 3] = 255;
    }
}

fs.writeFileSync('app/src/main/assets/atlas.png', PNG.sync.write(atlas));
fs.writeFileSync('app/src/main/assets/atlas_map.json', JSON.stringify(mapping, null, 4));
console.log('Packed', index, 'textures.');
