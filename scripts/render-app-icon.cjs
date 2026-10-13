#!/usr/bin/env node
// Regenerate committed application icon assets. Requires Node.js and sharp;
// neither dependency is required to build or run ShDisplayViewer.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const directory = path.resolve(__dirname, '../resources/application');
const svg = fs.readFileSync(path.join(directory, 'shdisplayviewer.svg'));
const sizes = [16, 24, 32, 48, 64, 128, 256, 512];
(async () => {
  const frames = [];
  for (const size of sizes) {
    const png = await sharp(svg, {density: 384}).resize(size, size).ensureAlpha().png().toBuffer();
    fs.writeFileSync(path.join(directory, `shdisplayviewer-${size}.png`), png);
    if (size <= 256) frames.push({size, png});
  }
  const table = Buffer.alloc(6 + frames.length * 16);
  table.writeUInt16LE(1, 2);
  table.writeUInt16LE(frames.length, 4);
  let offset = table.length;
  frames.forEach(({size, png}, i) => {
    const entry = 6 + i * 16;
    table[entry] = size === 256 ? 0 : size;
    table[entry + 1] = size === 256 ? 0 : size;
    table.writeUInt16LE(1, entry + 4);
    table.writeUInt16LE(32, entry + 6);
    table.writeUInt32LE(png.length, entry + 8);
    table.writeUInt32LE(offset, entry + 12);
    offset += png.length;
  });
  fs.writeFileSync(path.join(directory, 'shdisplayviewer.ico'), Buffer.concat([table, ...frames.map(f => f.png)]));
  process.stdout.write('Generated PNG sizes 16–512 and Windows ICO sizes 16–256.\n');
})().catch(error => { process.stderr.write(error.message + '\n'); process.exit(1); });
