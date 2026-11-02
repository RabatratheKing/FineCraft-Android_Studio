const fs = require('fs');
const atlasMap = JSON.parse(fs.readFileSync('app/src/main/assets/atlas_map.json'));
console.log("oak_leaves:", atlasMap["oak_leaves"]);
console.log("grass_block_top:", atlasMap["grass_block_top"]);
console.log("grass_block_side:", atlasMap["grass_block_side"]);
console.log("grass_block_bottom:", atlasMap["grass_block_bottom"]);
console.log("dirt:", atlasMap["dirt"]);
console.log("oak_log:", atlasMap["oak_log"]);
console.log("oak_log_top:", atlasMap["oak_log_top"]);
