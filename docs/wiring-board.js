/**
 * نقشه سیم‌کشی ESP32-S3 — مسیرهای منظم (orthogonal)
 */
(function () {
  const svg = document.getElementById('wiring-canvas');
  if (!svg) return;

  const NS = 'http://www.w3.org/2000/svg';

  const COLORS = {
    red: '#f85149',
    black: '#6e7681',
    orange: '#f0883e',
    yellow: '#d4a72c',
    green: '#3fb950',
    blue: '#58a6ff',
    purple: '#a371f7',
    cyan: '#79c0ff',
  };

  const ESP = {
    leftX: 478,
    rightX: 762,
    top: 110,
    pinStep: 24,
  };

  const LANES = { left: 320, right: 980 };

  const leftPins = [
    { gpio: '4', label: 'GPIO4', used: true, role: 'DHT11 DATA' },
    { gpio: '5', label: 'GPIO5', used: true, role: 'AP3216 SDA' },
    { gpio: '6', used: false },
    { gpio: '7', label: 'GPIO7', used: true, role: 'سطح آب S' },
    { gpio: '15', used: false },
    { gpio: '16', label: 'GPIO16', used: true, role: 'SIM RX ← TXD' },
    { gpio: '17', label: 'GPIO17', used: true, role: 'SIM TX → RXD' },
    { gpio: '18', label: 'GPIO18', used: true, role: 'SIM RST (اختیاری)' },
    { gpio: '8', used: false },
    { gpio: '3', used: false },
    { gpio: '46', used: false },
    { gpio: '9', label: 'GPIO9', used: true, role: 'AP3216 SCL' },
    { gpio: '10', label: 'GPIO10', used: true, role: '→ ULN2003 1B' },
    { gpio: '11', label: 'GPIO11', used: true, role: '→ ULN2003 2B' },
    { gpio: '12', used: false },
    { gpio: '5V', label: '5V', used: true, power: true },
    { gpio: 'GND', label: 'GND', used: true, gnd: true },
    { gpio: '3V3', label: '3.3V', used: true, power3: true },
  ];

  const rightPins = [
    { gpio: 'GND', used: true, gnd: true },
    { gpio: 'TX', used: false },
    { gpio: 'RX', used: false },
    { gpio: '1', label: 'GPIO1', used: true, role: 'خاک۱ AO' },
    { gpio: '2', label: 'GPIO2', used: true, role: 'خاک۲ AO' },
    { gpio: '42', used: false },
    { gpio: '41', used: false },
    { gpio: '40', used: false },
    { gpio: '39', used: false },
    { gpio: '38', used: false },
    { gpio: '37', used: false },
    { gpio: '36', used: false },
    { gpio: '35', used: false },
    { gpio: '0', used: false },
    { gpio: '45', used: false },
    { gpio: '48', used: false },
    { gpio: '47', used: false },
    { gpio: '21', used: false },
    { gpio: '20', used: false },
    { gpio: 'GND', used: true, gnd: true },
  ];

  function el(tag, attrs) {
    const e = document.createElementNS(NS, tag);
    if (attrs) Object.entries(attrs).forEach(([k, v]) => e.setAttribute(k, v));
    return e;
  }

  function text(x, y, str, opts = {}) {
    const t = el('text', {
      x, y,
      fill: opts.fill || '#8b949e',
      'font-size': opts.size || 10,
      'font-family': 'Consolas, Segoe UI, sans-serif',
      'text-anchor': opts.anchor || 'start',
    });
    if (opts.weight) t.setAttribute('font-weight', 'bold');
    t.textContent = str;
    return t;
  }

  function wirePath(x1, y1, x2, y2, laneX) {
    if (laneX !== undefined) {
      return `M ${x1} ${y1} H ${laneX} V ${y2} H ${x2}`;
    }
    const midY = (y1 + y2) / 2;
    return `M ${x1} ${y1} H ${(x1 + x2) / 2} V ${midY} H ${x2} V ${y2}`;
  }

  function drawWire(x1, y1, x2, y2, color, laneX, label, labelX, labelY) {
    const p = el('path', {
      d: wirePath(x1, y1, x2, y2, laneX),
      fill: 'none',
      stroke: color,
      'stroke-width': 2.5,
      'stroke-linecap': 'round',
      'stroke-linejoin': 'round',
    });
    svg.appendChild(p);
    if (label) {
      const bg = el('rect', {
        x: labelX - 2,
        y: labelY - 11,
        width: label.length * 5.5 + 8,
        height: 14,
        rx: 3,
        fill: '#0d1117',
        opacity: 0.85,
      });
      svg.appendChild(bg);
      svg.appendChild(text(labelX, labelY, label, { fill: color, size: 9 }));
    }
  }

  function bus(y, color, label, dashed) {
    const line = el('line', {
      x1: 30, y1: y, x2: 1250, y2: y,
      stroke: color,
      'stroke-width': dashed ? 2 : 4,
      'stroke-dasharray': dashed ? '8,5' : null,
    });
    svg.appendChild(line);
    svg.appendChild(text(640, y - 6, label, {
      anchor: 'middle',
      fill: color,
      size: 11,
      weight: true,
    }));
  }

  function moduleBox(x, y, w, h, title, stroke) {
    const g = el('g');
    g.appendChild(el('rect', {
      x, y, width: w, height: h, rx: 8,
      fill: '#21262d',
      stroke: stroke || '#30363d',
      'stroke-width': 2,
    }));
    g.appendChild(text(x + w / 2, y + 18, title, {
      anchor: 'middle',
      fill: stroke || '#e6edf3',
      size: 11,
      weight: true,
    }));
    svg.appendChild(g);
    return { x, y, w, h, portX: x + w };
  }

  function port(x, y, label, color) {
    svg.appendChild(el('circle', {
      cx: x, cy: y, r: 4,
      fill: color || '#8b949e',
      stroke: '#fff',
      'stroke-width': 1,
    }));
    svg.appendChild(text(x - 6, y + 4, label, {
      anchor: 'end',
      fill: '#c9d1d9',
      size: 9,
    }));
    return { x, y };
  }

  function portRight(box, offsetY, label, color) {
    const x = box.portX;
    const y = box.y + offsetY;
    svg.appendChild(el('circle', {
      cx: x, cy: y, r: 4,
      fill: color || '#8b949e',
      stroke: '#fff',
      'stroke-width': 1,
    }));
    svg.appendChild(text(x + 8, y + 4, label, {
      fill: '#c9d1d9',
      size: 9,
    }));
    return { x, y };
  }

  function portLeft(box, offsetY, label, color) {
    const x = box.x;
    const y = box.y + offsetY;
    svg.appendChild(el('circle', {
      cx: x, cy: y, r: 4,
      fill: color || '#8b949e',
      stroke: '#fff',
      'stroke-width': 1,
    }));
    svg.appendChild(text(x - 8, y + 4, label, {
      anchor: 'end',
      fill: '#c9d1d9',
      size: 9,
    }));
    return { x, y };
  }

  function pinY(index) {
    return ESP.top + index * ESP.pinStep + 12;
  }

  function getLeftPinY(gpio) {
    const i = leftPins.findIndex((p) => p.gpio === gpio);
    return i >= 0 ? pinY(i) : null;
  }

  function getRightPinY(gpio) {
    const i = rightPins.findIndex((p) => p.gpio === gpio);
    return i >= 0 ? pinY(i) : null;
  }

  // Background
  svg.appendChild(el('rect', {
    x: 0, y: 0, width: 1280, height: 880,
    fill: '#0d1117',
  }));

  // Power buses
  bus(42, COLORS.red, '+5V  پاوربانک (قرمز)');
  bus(58, COLORS.cyan, '+3.3V  از برد ESP (آبی خط‌چین)', true);
  bus(838, COLORS.black, 'GND مشترک (مشکی)');

  // Solar + powerbank
  const solar = moduleBox(500, 8, 90, 28, '☀ پنل 5V', '#f0883e');
  const bank = moduleBox(610, 8, 110, 28, 'پاوربانک 5V/2A', '#3fb950');
  drawWire(solar.portX, 22, bank.x, 22, COLORS.red);
  drawWire(bank.x + 55, 36, 640, 42, COLORS.red);

  // ESP32 board body
  const bx = 490;
  const by = 95;
  const bw = 280;
  const bh = 490;
  svg.appendChild(el('rect', {
    x: bx, y: by, width: bw, height: bh, rx: 10,
    fill: '#1a2332',
    stroke: '#58a6ff',
    'stroke-width': 3,
  }));
  svg.appendChild(el('rect', {
    x: bx + 50, y: by + 80, width: 180, height: 90, rx: 4,
    fill: '#2d3748',
    stroke: '#6e7681',
  }));
  svg.appendChild(text(bx + bw / 2, by + 125, 'ESP32-S3-N8R2', {
    anchor: 'middle',
    fill: '#e6edf3',
    size: 13,
    weight: true,
  }));
  svg.appendChild(text(bx + bw / 2, by + 142, '8MB Flash · 2MB PSRAM', {
    anchor: 'middle',
    fill: '#8b949e',
    size: 9,
  }));
  svg.appendChild(text(bx + bw / 2, by + bh + 22, 'ESP32-S3 DevKit — برد حیاط', {
    anchor: 'middle',
    fill: '#58a6ff',
    size: 12,
    weight: true,
  }));

  // Draw pin headers
  leftPins.forEach((p, i) => {
    const y = pinY(i);
    const used = p.used;
    const dotColor = p.power ? COLORS.red : p.power3 ? COLORS.cyan : p.gnd ? COLORS.black : used ? COLORS.orange : '#30363d';
    svg.appendChild(el('circle', {
      cx: ESP.leftX, cy: y, r: used ? 5 : 3,
      fill: used ? dotColor : '#21262d',
      stroke: used ? '#fff' : '#484f58',
      'stroke-width': 1,
    }));
    svg.appendChild(text(ESP.leftX - 10, y + 4, p.gpio, {
      anchor: 'end',
      fill: used ? '#e6edf3' : '#484f58',
      size: 9,
      weight: used,
    }));
    if (p.label && used) {
      svg.appendChild(text(ESP.leftX + 12, y + 4, p.role || p.label, {
        fill: '#8b949e',
        size: 8,
      }));
    }
  });

  rightPins.forEach((p, i) => {
    const y = pinY(i);
    const used = p.used;
    svg.appendChild(el('circle', {
      cx: ESP.rightX, cy: y, r: used ? 5 : 3,
      fill: p.gnd ? COLORS.black : used ? COLORS.orange : '#21262d',
      stroke: used ? '#fff' : '#484f58',
      'stroke-width': 1,
    }));
    svg.appendChild(text(ESP.rightX + 10, y + 4, p.gpio, {
      fill: used ? '#e6edf3' : '#484f58',
      size: 9,
      weight: used,
    }));
    if (p.label && used) {
      svg.appendChild(text(ESP.rightX - 12, y + 4, p.role || p.label, {
        anchor: 'end',
        fill: '#8b949e',
        size: 8,
      }));
    }
  });

  // Modules
  const sim = moduleBox(24, 70, 200, 168, 'SIM800L EVB', '#f0883e');
  const soil1 = moduleBox(24, 258, 200, 95, 'رطوبت خاک ۱ (HW-103)', '#3fb950');
  const soil2 = moduleBox(24, 378, 200, 95, 'رطوبت خاک ۲ (HW-103)', '#3fb950');
  const water = moduleBox(24, 498, 200, 88, 'سنسور سطح آب', '#58a6ff');

  const dht = moduleBox(1096, 88, 200, 88, 'DHT11', '#58a6ff');
  const light = moduleBox(1096, 208, 200, 108, 'AP3216 نور', '#a371f7');
  const uln = moduleBox(900, 520, 180, 195, 'ULN2003AN', '#e3b341');
  const relay1 = moduleBox(1096, 548, 200, 82, 'رله ۱ (پمپ ۱)', '#3fb950');
  const relay2 = moduleBox(1096, 648, 200, 82, 'رله ۲ (پمپ ۲)', '#3fb950');
  const pump3v = moduleBox(1096, 748, 200, 52, 'منبع 3V پمپ‌ها', '#d4a72c');

  // SIM ports — ترتیب واقعی: 5VIN GND VDD TXD RXD GND RST
  const sim5 = portRight(sim, 28, '5VIN', COLORS.red);
  const simGnd1 = portRight(sim, 44, 'GND', COLORS.black);
  const simVdd = portRight(sim, 60, 'VDD', COLORS.cyan);
  const simTx = portRight(sim, 76, 'TXD', COLORS.yellow);
  const simRx = portRight(sim, 92, 'RXD', COLORS.green);
  const simGnd2 = portRight(sim, 108, 'GND', COLORS.black);
  const simRst = portRight(sim, 124, 'RST', COLORS.orange);

  drawWire(sim5.x, sim5.y, 640, 42, COLORS.red, undefined, 'قرمز', 400, 35);
  drawWire(simGnd1.x, simGnd1.y, 640, 838, COLORS.black, LANES.left);
  drawWire(simVdd.x, simVdd.y, 200, 58, COLORS.cyan, LANES.left, '3.3V', 230, 52);
  drawWire(200, 58, getLeftPinY('3V3'), ESP.leftX, COLORS.cyan, LANES.left);
  drawWire(simTx.x, simTx.y, ESP.leftX, getLeftPinY('16'), COLORS.yellow, LANES.left, 'زرد→GPIO16', 260, 278);
  drawWire(simRx.x, simRx.y, ESP.leftX, getLeftPinY('17'), COLORS.green, LANES.left, 'سبز→GPIO17', 260, 304);
  drawWire(simGnd2.x, simGnd2.y, 640, 838, COLORS.black, LANES.left);
  drawWire(simRst.x, simRst.y, ESP.leftX, getLeftPinY('18'), COLORS.orange, LANES.left, 'نارنجی→GPIO18', 260, 330);

  // Soil 1
  const s1v = portRight(soil1, 32, 'VCC', COLORS.cyan);
  const s1ao = portRight(soil1, 52, 'AO', COLORS.orange);
  const s1g = portRight(soil1, 72, 'GND', COLORS.black);
  drawWire(s1v.x, s1v.y, 200, 58, COLORS.cyan, LANES.left);
  drawWire(s1ao.x, s1ao.y, ESP.rightX, getRightPinY('1'), COLORS.orange, LANES.left, 'نارنجی→GPIO1', 400, 248);
  drawWire(s1g.x, s1g.y, 640, 838, COLORS.black, LANES.left);

  // Soil 2
  const s2v = portRight(soil2, 32, 'VCC', COLORS.cyan);
  const s2ao = portRight(soil2, 52, 'AO', COLORS.orange);
  const s2g = portRight(soil2, 72, 'GND', COLORS.black);
  drawWire(s2v.x, s2v.y, 200, 58, COLORS.cyan, LANES.left);
  drawWire(s2ao.x, s2ao.y, ESP.rightX, getRightPinY('2'), COLORS.orange, LANES.left, 'نارنجی→GPIO2', 400, 274);
  drawWire(s2g.x, s2g.y, 640, 838, COLORS.black, LANES.left);

  // Water
  const wv = portRight(water, 28, '+', COLORS.cyan);
  const ws = portRight(water, 48, 'S', COLORS.orange);
  const wg = portRight(water, 68, '−', COLORS.black);
  drawWire(wv.x, wv.y, 200, 58, COLORS.cyan, LANES.left);
  drawWire(ws.x, ws.y, ESP.leftX, getLeftPinY('7'), COLORS.orange, LANES.left, 'نارنجی→GPIO7', 260, 488);
  drawWire(wg.x, wg.y, 640, 838, COLORS.black, LANES.left);

  // DHT11 (left ports on module)
  const dv = portLeft(dht, 32, 'VCC', COLORS.cyan);
  const dd = portLeft(dht, 52, 'DATA', COLORS.orange);
  const dg = portLeft(dht, 72, 'GND', COLORS.black);
  drawWire(1096, dv.y, 1080, 58, COLORS.cyan, LANES.right);
  drawWire(1080, 58, 200, 58, COLORS.cyan, LANES.right);
  drawWire(dd.x, dd.y, ESP.leftX, getLeftPinY('4'), COLORS.orange, LANES.right, 'نارنجی→GPIO4', 820, 128);
  drawWire(dg.x, dg.y, 640, 838, COLORS.black, LANES.right);

  // AP3216
  const lv = portLeft(light, 32, 'VCC', COLORS.cyan);
  const lsda = portLeft(light, 52, 'SDA', COLORS.blue);
  const lscl = portLeft(light, 72, 'SCL', COLORS.purple);
  const lg = portLeft(light, 92, 'GND', COLORS.black);
  drawWire(1096, lv.y, 1080, 58, COLORS.cyan, LANES.right);
  drawWire(lsda.x, lsda.y, ESP.leftX, getLeftPinY('5'), COLORS.blue, LANES.right, 'آبی→GPIO5', 820, 388);
  drawWire(lscl.x, lscl.y, ESP.leftX, getLeftPinY('9'), COLORS.purple, LANES.right, 'بنفش→GPIO9', 820, 414);
  drawWire(lg.x, lg.y, 640, 838, COLORS.black, LANES.right);

  // ULN2003 ports (right side outputs to relay, left side inputs from ESP)
  const uln1b = portLeft(uln, 36, '1B ← GPIO10', COLORS.orange);
  const uln2b = portLeft(uln, 56, '2B ← GPIO11', COLORS.orange);
  const ulnGnd = portLeft(uln, 76, '8 GND', COLORS.black);
  const ulnCom = portLeft(uln, 96, '9 COM→5V', COLORS.red);
  const uln1c = portRight(uln, 36, '1C→رله۱', COLORS.orange);
  const uln2c = portRight(uln, 56, '2C→رله۲', COLORS.orange);

  drawWire(ESP.leftX, getLeftPinY('10'), uln1b.x, uln1b.y, COLORS.orange, LANES.right, 'GPIO10→1B', 720, 448);
  drawWire(ESP.leftX, getLeftPinY('11'), uln2b.x, uln2b.y, COLORS.orange, LANES.right, 'GPIO11→2B', 720, 472);
  drawWire(ulnGnd.x, ulnGnd.y, 640, 838, COLORS.black, LANES.right);
  drawWire(ulnCom.x, ulnCom.y, 1080, 42, COLORS.red, LANES.right);
  drawWire(1080, 42, 640, 42, COLORS.red, LANES.right);

  svg.appendChild(text(uln.x + uln.w / 2, uln.y + 130, 'درایور رله — خروجی سینک', {
    anchor: 'middle',
    fill: '#8b949e',
    size: 8,
  }));
  svg.appendChild(text(uln.x + uln.w / 2, uln.y + 145, 'GPIO HIGH = رله ON (ماژول LOW-trigger)', {
    anchor: 'middle',
    fill: '#e3b341',
    size: 8,
  }));

  // Relay 1 (IN from ULN 1C)
  const r1v = portLeft(relay1, 28, 'VCC', COLORS.red);
  const r1in = portLeft(relay1, 48, 'IN', COLORS.orange);
  const r1g = portLeft(relay1, 68, 'GND', COLORS.black);
  drawWire(1096, r1v.y, 1080, 42, COLORS.red, LANES.right);
  drawWire(uln1c.x, uln1c.y, r1in.x, r1in.y, COLORS.orange, LANES.right, '1C→IN', 1000, 560);
  drawWire(r1g.x, r1g.y, 640, 838, COLORS.black, LANES.right);

  // Relay 2 (IN from ULN 2C)
  const r2v = portLeft(relay2, 28, 'VCC', COLORS.red);
  const r2in = portLeft(relay2, 48, 'IN', COLORS.orange);
  const r2g = portLeft(relay2, 68, 'GND', COLORS.black);
  drawWire(1096, r2v.y, 1080, 42, COLORS.red, LANES.right);
  drawWire(uln2c.x, uln2c.y, r2in.x, r2in.y, COLORS.orange, LANES.right, '2C→IN', 1000, 660);
  drawWire(r2g.x, r2g.y, 640, 838, COLORS.black, LANES.right);

  // ESP 5V and GND
  drawWire(640, 42, ESP.leftX, getLeftPinY('5V'), COLORS.red);
  drawWire(ESP.leftX, getLeftPinY('GND'), 640, 838, COLORS.black);
  drawWire(ESP.rightX, getRightPinY('GND'), 640, 838, COLORS.black);

  // 3.3V rail to left side sensors (shared tap)
  drawWire(ESP.leftX, getLeftPinY('3V3'), 200, 58, COLORS.cyan, LANES.left);

  // Pump power note
  svg.appendChild(text(1096 + 100, 778, 'پمپ ۳V: COM←منبع +  |  NO→پمپ +', {
    anchor: 'middle',
    fill: '#d4a72c',
    size: 9,
  }));
  svg.appendChild(text(1096 + 100, 792, 'پمپ − → GND منبع ۳V', {
    anchor: 'middle',
    fill: '#8b949e',
    size: 8,
  }));

  // Capacitor note near bottom of SIM box
  svg.appendChild(el('rect', {
    x: sim.x + 10, y: sim.y + 140, width: 100, height: 22, rx: 4,
    fill: '#2a3310',
    stroke: '#d4a72c',
  }));
  svg.appendChild(text(sim.x + 60, sim.y + 154, '1000µF: +5VIN −GND', {
    anchor: 'middle',
    fill: '#e3b341',
    size: 7,
  }));
})();
