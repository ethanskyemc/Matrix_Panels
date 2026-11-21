// Zero-dependency pure-JS GIF87a / GIF89a LZW frame decoder
// Allows animated GIF upload, cropping, and previewing completely offline on phones!

(function (global) {
  function decodeLZW(minCodeSize, data, pixelCount) {
    const clearCode = 1 << minCodeSize;
    const eoiCode = clearCode + 1;
    let codeSize = minCodeSize + 1;
    let codeMask = (1 << codeSize) - 1;
    let nextCode = eoiCode + 1;

    const prefix = new Int32Array(4096);
    const suffix = new Uint8Array(4096);
    const stack = new Uint8Array(4096);

    for (let i = 0; i < clearCode; i++) {
      prefix[i] = -1;
      suffix[i] = i;
    }

    const out = new Uint8Array(pixelCount);
    let outPos = 0;
    let bitBuf = 0;
    let bitCount = 0;
    let dataPos = 0;
    let oldCode = -1;
    let firstChar = 0;

    function nextByte() {
      return dataPos < data.length ? data[dataPos++] : 0;
    }

    while (outPos < pixelCount) {
      while (bitCount < codeSize) {
        if (dataPos >= data.length) return out;
        bitBuf |= nextByte() << bitCount;
        bitCount += 8;
      }
      const code = bitBuf & codeMask;
      bitBuf >>= codeSize;
      bitCount -= codeSize;

      if (code === clearCode) {
        codeSize = minCodeSize + 1;
        codeMask = (1 << codeSize) - 1;
        nextCode = eoiCode + 1;
        oldCode = -1;
        continue;
      }
      if (code === eoiCode) break;

      let inCode = code;
      let sp = 0;
      if (code >= nextCode) {
        if (oldCode === -1) break;
        stack[sp++] = firstChar;
        inCode = oldCode;
      }
      while (inCode >= clearCode) {
        stack[sp++] = suffix[inCode];
        inCode = prefix[inCode];
        if (sp >= 4096) break;
      }
      firstChar = suffix[inCode];
      stack[sp++] = firstChar;

      if (oldCode !== -1 && nextCode < 4096) {
        prefix[nextCode] = oldCode;
        suffix[nextCode] = firstChar;
        nextCode++;
        if (nextCode > codeMask && codeSize < 12) {
          codeSize++;
          codeMask = (1 << codeSize) - 1;
        }
      }
      oldCode = code;

      while (sp > 0 && outPos < pixelCount) {
        out[outPos++] = stack[--sp];
      }
    }
    return out;
  }

  function decodeGif(arrayBuffer) {
    const u8 = new Uint8Array(arrayBuffer);
    let pos = 0;
    const readU16 = () => { const v = u8[pos] | (u8[pos + 1] << 8); pos += 2; return v; };
    const readSubBlocks = () => {
      const chunks = [];
      let total = 0;
      while (pos < u8.length) {
        const len = u8[pos++];
        if (len === 0) break;
        chunks.push(u8.subarray(pos, pos + len));
        total += len;
        pos += len;
      }
      const out = new Uint8Array(total);
      let off = 0;
      for (const ch of chunks) { out.set(ch, off); off += ch.length; }
      return out;
    };

    const sig = String.fromCharCode(u8[0], u8[1], u8[2]);
    if (sig !== 'GIF') throw new Error('Not a GIF file');
    pos = 6;

    const width = readU16();
    const height = readU16();
    const packed = u8[pos++];
    pos += 2; // bgColorIndex, aspect

    let gct = null;
    if (packed & 0x80) {
      const size = 1 << ((packed & 0x07) + 1);
      gct = u8.subarray(pos, pos + size * 3);
      pos += size * 3;
    }

    const canvas = new Uint8ClampedArray(width * height * 4);
    const frames = [];
    let delayMs = 100;
    let disposal = 0;
    let transparentIndex = -1;

    while (pos < u8.length) {
      const intro = u8[pos++];
      if (intro === 0x3B) break; // Trailer

      if (intro === 0x21) { // Extension
        const label = u8[pos++];
        if (label === 0xF9) { // Graphic Control Extension
          pos++; // block size (4)
          const gPacked = u8[pos++];
          disposal = (gPacked >> 2) & 0x07;
          const hasTrans = (gPacked & 0x01) !== 0;
          delayMs = Math.max(20, readU16() * 10);
          transparentIndex = hasTrans ? u8[pos++] : (pos++, -1);
          pos++; // terminator
        } else {
          readSubBlocks();
        }
      } else if (intro === 0x2C) { // Image Descriptor
        const left = readU16();
        const top = readU16();
        const fw = readU16();
        const fh = readU16();
        const imgPacked = u8[pos++];
        const interlaced = (imgPacked & 0x40) !== 0;

        let ct = gct;
        if (imgPacked & 0x80) {
          const size = 1 << ((imgPacked & 0x07) + 1);
          ct = u8.subarray(pos, pos + size * 3);
          pos += size * 3;
        }
        const minCodeSize = u8[pos++];
        const lzwData = readSubBlocks();
        const indices = decodeLZW(minCodeSize, lzwData, fw * fh);

        const prevSnapshot = disposal === 3 ? new Uint8ClampedArray(canvas) : null;

        const interlaceStarts = [0, 4, 2, 1];
        const interlaceSteps = [8, 8, 4, 2];

        const writeRow = (srcRow, dstRow) => {
          const py = top + dstRow;
          if (py < 0 || py >= height) return;
          for (let x = 0; x < fw; x++) {
            const px = left + x;
            if (px < 0 || px >= width) continue;
            const ci = indices[srcRow * fw + x];
            if (ci === transparentIndex || !ct) continue;
            const dst = (py * width + px) * 4;
            canvas[dst]     = ct[ci * 3];
            canvas[dst + 1] = ct[ci * 3 + 1];
            canvas[dst + 2] = ct[ci * 3 + 2];
            canvas[dst + 3] = 255;
          }
        };

        if (interlaced) {
          let srcRow = 0;
          for (let pass = 0; pass < 4; pass++) {
            for (let y = interlaceStarts[pass]; y < fh; y += interlaceSteps[pass]) {
              writeRow(srcRow++, y);
            }
          }
        } else {
          for (let y = 0; y < fh; y++) writeRow(y, y);
        }

        frames.push({
          width,
          height,
          delayMs,
          rgba: new Uint8ClampedArray(canvas)
        });

        if (disposal === 2) {
          for (let y = 0; y < fh; y++) {
            for (let x = 0; x < fw; x++) {
              const px = left + x, py = top + y;
              if (px >= 0 && px < width && py >= 0 && py < height) {
                const dst = (py * width + px) * 4;
                canvas[dst] = canvas[dst + 1] = canvas[dst + 2] = canvas[dst + 3] = 0;
              }
            }
          }
        } else if (disposal === 3 && prevSnapshot) {
          canvas.set(prevSnapshot);
        }

        delayMs = 100;
        disposal = 0;
        transparentIndex = -1;
      } else {
        break;
      }
    }

    return { width, height, frames };
  }

  global.GifDecoder = { decodeGif };
})(window);
