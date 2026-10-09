/**
 * نقشه سیم‌کشی تفکیکی — هر ماژول جدا
 */
(function () {
  const root = document.getElementById('module-wiring-root');
  if (!root) return;

  const NS = 'http://www.w3.org/2000/svg';
  const C = {
    red: '#f85149',
    black: '#6e7681',
    orange: '#f0883e',
    yellow: '#d4a72c',
    green: '#3fb950',
    blue: '#58a6ff',
    purple: '#a371f7',
    cyan: '#79c0ff',
    gold: '#e3b341',
    text: '#c9d1d9',
    muted: '#8b949e',
    bg: '#0d1117',
    box: '#21262d',
  };

  const MODULE_X = 24;
  const MODULE_W = 200;
  const MODULE_PORT_X = MODULE_X + MODULE_W;
  const LANE = 320;
  const ESP_X = 500;
  const ESP_W = 200;
  const ESP_PORT_X = ESP_X;

  function el(tag, attrs) {
    const e = document.createElementNS(NS, tag);
    if (attrs) Object.entries(attrs).forEach(([k, v]) => {
      if (v != null) e.setAttribute(k, String(v));
    });
    return e;
  }

  function txt(x, y, str, opts = {}) {
    const t = el('text', {
      x, y,
      fill: opts.fill || C.muted,
      'font-size': opts.size || 11,
      'font-family': 'Consolas, Segoe UI, sans-serif',
      'text-anchor': opts.anchor || 'start',
      'font-weight': opts.bold ? 'bold' : 'normal',
    });
    t.textContent = str;
    return t;
  }

  function wire(svg, x1, y1, x2, y2, color, label) {
    const lane = LANE;
    const d = `M ${x1} ${y1} H ${lane} V ${y2} H ${x2}`;
    svg.appendChild(el('path', {
      d, fill: 'none', stroke: color, 'stroke-width': 2.5,
      'stroke-linecap': 'round', 'stroke-linejoin': 'round',
    }));
    if (label) {
      const lx = lane + 6;
      const ly = (y1 + y2) / 2 + 4;
      svg.appendChild(el('rect', {
        x: lx - 2, y: ly - 12, width: label.length * 6.2 + 8, height: 16,
        rx: 3, fill: C.bg, opacity: 0.9,
      }));
      svg.appendChild(txt(lx, ly, label, { fill: color, size: 9 }));
    }
  }

  function wireDirect(svg, x1, y1, x2, y2, color, label) {
    const midX = (x1 + x2) / 2;
    const d = `M ${x1} ${y1} H ${midX} V ${y2} H ${x2}`;
    svg.appendChild(el('path', {
      d, fill: 'none', stroke: color, 'stroke-width': 2.5,
      'stroke-linecap': 'round', 'stroke-linejoin': 'round',
    }));
    if (label) {
      svg.appendChild(txt(midX + 4, (y1 + y2) / 2, label, { fill: color, size: 9 }));
    }
  }

  function port(x, y, color) {
    return el('circle', {
      cx: x, cy: y, r: 5, fill: color, stroke: '#fff', 'stroke-width': 1.5,
    });
  }

  function moduleBox(svg, y, h, title, stroke, subtitle) {
    svg.appendChild(el('rect', {
      x: MODULE_X, y, width: MODULE_W, height: h, rx: 8,
      fill: C.box, stroke: stroke || '#30363d', 'stroke-width': 2,
    }));
    svg.appendChild(txt(MODULE_X + MODULE_W / 2, y + 20, title, {
      anchor: 'middle', fill: stroke || C.text, size: 12, bold: true,
    }));
    if (subtitle) {
      svg.appendChild(txt(MODULE_X + MODULE_W / 2, y + 36, subtitle, {
        anchor: 'middle', fill: C.muted, size: 9,
      }));
    }
  }

  function espBox(svg, y, h, pins) {
    svg.appendChild(el('rect', {
      x: ESP_X, y, width: ESP_W, height: h, rx: 8,
      fill: '#1a2332', stroke: '#58a6ff', 'stroke-width': 2,
    }));
    svg.appendChild(txt(ESP_X + ESP_W / 2, y + 18, 'ESP32-S3 DevKit', {
      anchor: 'middle', fill: '#58a6ff', size: 11, bold: true,
    }));
    svg.appendChild(txt(ESP_X + ESP_W / 2, y + 32, 'پین‌های این ماژول', {
      anchor: 'middle', fill: C.muted, size: 9,
    }));
    pins.forEach((p, i) => {
      const py = y + 52 + i * 28;
      svg.appendChild(port(ESP_PORT_X, py, p.color || C.orange));
      svg.appendChild(txt(ESP_PORT_X + 12, py + 4, p.label, { fill: C.text, size: 10, bold: true }));
      if (p.hint) svg.appendChild(txt(ESP_PORT_X + 12, py + 16, p.hint, { fill: C.muted, size: 8 }));
      p._y = py;
    });
    return pins;
  }

  function modulePin(svg, y, label, color, hint) {
    svg.appendChild(port(MODULE_PORT_X, y, color));
    svg.appendChild(txt(MODULE_X + 10, y + 4, label, { fill: C.text, size: 10 }));
    if (hint) svg.appendChild(txt(MODULE_X + 10, y + 16, hint, { fill: C.muted, size: 8 }));
    return y;
  }

  function busLine(svg, y, color, label, x1, x2) {
    svg.appendChild(el('line', {
      x1, y1: y, x2, y2: y, stroke: color, 'stroke-width': 3,
    }));
    svg.appendChild(txt((x1 + x2) / 2, y - 6, label, {
      anchor: 'middle', fill: color, size: 10, bold: true,
    }));
  }

  function createCard(config) {
    const card = document.createElement('div');
    card.className = 'module-diagram-card';
    card.innerHTML = `<h3 style="color:${config.color || '#58a6ff'}">${config.title}</h3>
      <p class="module-desc">${config.desc}</p>`;

    const wrap = document.createElement('div');
    wrap.className = 'module-svg-wrap';
    const svg = el('svg', {
      viewBox: `0 0 720 ${config.height || 220}`,
      'aria-label': config.title,
    });
    svg.appendChild(el('rect', {
      x: 0, y: 0, width: 720, height: config.height || 220, fill: C.bg,
    }));
    config.draw(svg);
    wrap.appendChild(svg);
    card.appendChild(wrap);

    if (config.rows) {
      const table = document.createElement('table');
      table.className = 'pin-table';
      table.innerHTML = '<thead><tr><th>پین ماژول</th><th>رنگ</th><th>اتصال</th></tr></thead>';
      const tbody = document.createElement('tbody');
      config.rows.forEach((r) => {
        const tr = document.createElement('tr');
        tr.innerHTML = `<td>${r[0]}</td><td>${r[1]}</td><td>${r[2]}</td>`;
        tbody.appendChild(tr);
      });
      table.appendChild(tbody);
      card.appendChild(table);
    }
    root.appendChild(card);
  }

  // ─── 1. تامین برق ───
  createCard({
    title: '۱. تامین برق خودکفا',
    color: '#3fb950',
    desc: 'پنل خورشیدی → پاوربانک → توزیع 5V و GND. SIM800L و رله‌ها از پاوربانک؛ پمپ‌ها از منبع 3V جدا. خازن ۱۰۰۰µF فقط روی SIM.',
    height: 360,
    rows: [
      ['پنل 5V +', 'قرمز', 'ورودی شارژ پاوربانک'],
      ['پنل GND', 'مشکی', 'GND مشترک'],
      ['پاوربانک OUT +', 'قرمز', 'ESP 5V · SIM 5VIN · رله VCC · ULN COM'],
      ['پاوربانک OUT −', 'مشکی', 'GND مشترک همه ماژول‌ها'],
      ['خازن ۱۰۰۰µF +', 'قرمز', 'موازی روی SIM 5VIN'],
      ['خازن ۱۰۰۰µF −', 'مشکی', 'موازی روی SIM GND'],
      ['منبع 3V +', 'قرمز', 'COM رله‌ها (پمپ)'],
      ['منبع 3V −', 'مشکی', 'GND پمپ‌ها'],
    ],
    draw(svg) {
      busLine(svg, 28, C.red, '+5V BUS', 20, 700);
      busLine(svg, 340, C.black, 'GND BUS', 20, 700);

      // Solar
      svg.appendChild(el('rect', {
        x: 24, y: 48, width: 150, height: 70, rx: 6,
        fill: C.box, stroke: '#f0883e', 'stroke-width': 2,
      }));
      svg.appendChild(txt(99, 70, '☀ پنل 5V', { anchor: 'middle', fill: '#f0883e', size: 11, bold: true }));
      svg.appendChild(txt(99, 90, '+ شارژ پاوربانک', { anchor: 'middle', fill: C.red, size: 9 }));
      svg.appendChild(txt(99, 106, '− GND', { anchor: 'middle', fill: C.black, size: 9 }));

      // Powerbank
      svg.appendChild(el('rect', {
        x: 200, y: 48, width: 170, height: 70, rx: 6,
        fill: C.box, stroke: '#3fb950', 'stroke-width': 2,
      }));
      svg.appendChild(txt(285, 70, '🔌 پاوربانک', { anchor: 'middle', fill: '#3fb950', size: 11, bold: true }));
      svg.appendChild(txt(285, 90, 'OUT 5V / 2A', { anchor: 'middle', fill: C.muted, size: 9 }));
      svg.appendChild(txt(285, 106, 'GND مشترک', { anchor: 'middle', fill: C.muted, size: 9 }));
      wireDirect(svg, 174, 70, 200, 70, C.red, 'شارژ');

      const loads = [
        { y: 48, label: 'ESP32 5V', color: '#58a6ff' },
        { y: 82, label: 'SIM 5VIN + خازن', color: '#f0883e' },
        { y: 116, label: 'رله ۱+۲ VCC', color: '#3fb950' },
        { y: 150, label: 'ULN COM پین ۹', color: C.gold },
      ];
      loads.forEach((l) => {
        svg.appendChild(el('rect', {
          x: 420, y: l.y, width: 150, height: 28, rx: 4,
          fill: C.box, stroke: l.color, 'stroke-width': 1,
        }));
        svg.appendChild(txt(495, l.y + 18, l.label, { anchor: 'middle', fill: l.color, size: 9 }));
        wireDirect(svg, 370, 28, 420, l.y + 14, C.red);
        wireDirect(svg, 570, l.y + 14, 620, 340, C.black);
      });
      wireDirect(svg, 285, 48, 370, 28, C.red);

      // 3V pump supply — separate, not on 5V bus
      svg.appendChild(el('rect', {
        x: 24, y: 210, width: 200, height: 70, rx: 6,
        fill: C.box, stroke: '#d4a72c', 'stroke-width': 2,
      }));
      svg.appendChild(txt(124, 232, 'منبع 3V پمپ‌ها', { anchor: 'middle', fill: '#d4a72c', size: 11, bold: true }));
      svg.appendChild(txt(124, 252, 'جدا از ESP و SIM', { anchor: 'middle', fill: C.muted, size: 9 }));
      svg.appendChild(txt(124, 268, '+ → COM رله · − → GND پمپ', { anchor: 'middle', fill: C.muted, size: 8 }));
      svg.appendChild(el('rect', {
        x: 260, y: 220, width: 110, height: 50, rx: 4,
        fill: C.box, stroke: '#d4a72c',
      }));
      svg.appendChild(txt(315, 250, 'پمپ ۱ + ۲', { anchor: 'middle', fill: '#d4a72c', size: 10 }));
      wireDirect(svg, 224, 245, 260, 245, C.red, '3V');
    },
  });

  // ─── 2. SIM800L ───
  createCard({
    title: '۲. SIM800L EVB',
    color: '#f0883e',
    desc: 'ترتیب پین‌ها روی برد تو: 5VIN · GND · VDD · TXD · RXD · GND · RST. UART متقاطع. خازن ۱۰۰۰µF موازی بین 5VIN و GND.',
    height: 340,
    rows: [
      ['5VIN', 'قرمز', 'پاوربانک 5V (+ خازن ۱۰۰۰µF)'],
      ['GND (اول)', 'مشکی', 'GND مشترک + − خازن'],
      ['VDD', 'آبی روشن', 'ESP 3.3V (سطح منطقی)'],
      ['TXD', 'زرد', 'ESP GPIO 16 (RX)'],
      ['RXD', 'سبز', 'ESP GPIO 17 (TX)'],
      ['GND (دوم)', 'مشکی', 'همان GND مشترک (هر دو یکی‌اند)'],
      ['RST', 'نارنجی', 'ESP GPIO 18 (اختیاری)'],
    ],
    draw(svg) {
      moduleBox(svg, 24, 250, 'SIM800L EVB', '#f0883e', '5VIN GND VDD TXD RXD GND RST');
      const pins = [
        { y: 58, label: '5VIN', c: C.red, kind: 'bus', wire: C.red, lbl: 'قرمز 5V' },
        { y: 88, label: 'GND', c: C.black, kind: 'gnd', wire: C.black, lbl: 'مشکی' },
        { y: 118, label: 'VDD', c: C.cyan, kind: '3v3', wire: C.cyan, lbl: 'آبی' },
        { y: 148, label: 'TXD', c: C.yellow, kind: 'tx', wire: C.yellow, lbl: 'زرد' },
        { y: 178, label: 'RXD', c: C.green, kind: 'rx', wire: C.green, lbl: 'سبز' },
        { y: 208, label: 'GND', c: C.black, kind: 'gnd', wire: C.black, lbl: 'مشکی' },
        { y: 238, label: 'RST', c: C.orange, kind: 'rst', wire: C.orange, lbl: 'نارنجی' },
      ];
      const ep = espBox(svg, 24, 250, [
        { label: 'GPIO 16 (RX)', hint: '← SIM TXD', color: C.yellow },
        { label: 'GPIO 17 (TX)', hint: '→ SIM RXD', color: C.green },
        { label: 'GPIO 18', hint: '→ SIM RST', color: C.orange },
        { label: '3.3V', color: C.cyan },
        { label: 'GND', color: C.black },
      ]);
      busLine(svg, 12, C.red, '+5V پاوربانک', 220, 700);
      pins.forEach((p) => {
        modulePin(svg, p.y, p.label, p.c);
        if (p.kind === 'bus') wireDirect(svg, MODULE_PORT_X, p.y, 360, 12, p.wire, p.lbl);
        else if (p.kind === 'tx') wire(svg, MODULE_PORT_X, p.y, ESP_PORT_X, ep[0]._y, p.wire, p.lbl);
        else if (p.kind === 'rx') wire(svg, MODULE_PORT_X, p.y, ESP_PORT_X, ep[1]._y, p.wire, p.lbl);
        else if (p.kind === 'rst') wire(svg, MODULE_PORT_X, p.y, ESP_PORT_X, ep[2]._y, p.wire, p.lbl);
        else if (p.kind === '3v3') wire(svg, MODULE_PORT_X, p.y, ESP_PORT_X, ep[3]._y, p.wire, p.lbl);
        else wire(svg, MODULE_PORT_X, p.y, ESP_PORT_X, ep[4]._y, p.wire, p.lbl);
      });
      svg.appendChild(el('rect', {
        x: 40, y: 288, width: 200, height: 40, rx: 6,
        fill: '#2a3310', stroke: '#d4a72c', 'stroke-width': 2,
      }));
      svg.appendChild(txt(140, 305, 'خازن 1000µF موازی', { anchor: 'middle', fill: '#e3b341', size: 10, bold: true }));
      svg.appendChild(txt(140, 320, '+ → 5VIN     − → هر GND', { anchor: 'middle', fill: C.muted, size: 9 }));
      svg.appendChild(txt(480, 310, 'دو GND یکی‌اند — هر دو به مشترک', {
        anchor: 'middle', fill: '#f0883e', size: 10,
      }));
    },
  });

  // ─── 3. خاک ۱ ───
  createCard({
    title: '۳. رطوبت خاک ۱ (HW-103 + HW-080)',
    color: '#3fb950',
    desc: 'گیاه ۱ — فقط پین AO (آنالوگ). DO استفاده نشود.',
    rows: [
      ['VCC', 'آبی روشن', 'ESP 3.3V'],
      ['GND', 'مشکی', 'GND مشترک'],
      ['AO', 'نارنجی', 'ESP GPIO 1'],
      ['DO', '—', 'استفاده نکن'],
    ],
    draw(svg) {
      moduleBox(svg, 40, 130, 'ماژول HW-103', '#3fb950', 'پروب HW-080 در خاک');
      const ep = espBox(svg, 40, 100, [
        { label: 'GPIO 1', hint: 'ADC · گیاه ۱', color: C.orange },
        { label: '3.3V', color: C.cyan },
        { label: 'GND', color: C.black },
      ]);
      const yV = 80, yG = 110, yA = 140;
      modulePin(svg, yV, 'VCC', C.cyan);
      modulePin(svg, yG, 'GND', C.black);
      modulePin(svg, yA, 'AO', C.orange);
      wire(svg, MODULE_PORT_X, yA, ESP_PORT_X, ep[0]._y, C.orange, 'نارنجی');
      wire(svg, MODULE_PORT_X, yV, ESP_PORT_X, ep[1]._y, C.cyan, '3.3V');
      wire(svg, MODULE_PORT_X, yG, ESP_PORT_X, ep[2]._y, C.black, 'GND');
    },
  });

  // ─── 4. خاک ۲ ───
  createCard({
    title: '۴. رطوبت خاک ۲ (HW-103 + HW-080)',
    color: '#3fb950',
    desc: 'گیاه ۲ — همان مدل سنسور، پین جدا روی ESP.',
    rows: [
      ['VCC', 'آبی روشن', 'ESP 3.3V'],
      ['GND', 'مشکی', 'GND مشترک'],
      ['AO', 'نارنجی', 'ESP GPIO 2'],
    ],
    draw(svg) {
      moduleBox(svg, 40, 130, 'ماژول HW-103', '#3fb950', 'پروب HW-080 در خاک');
      const ep = espBox(svg, 40, 100, [
        { label: 'GPIO 2', hint: 'ADC · گیاه ۲', color: C.orange },
        { label: '3.3V', color: C.cyan },
        { label: 'GND', color: C.black },
      ]);
      const yV = 80, yG = 110, yA = 140;
      modulePin(svg, yV, 'VCC', C.cyan);
      modulePin(svg, yG, 'GND', C.black);
      modulePin(svg, yA, 'AO', C.orange);
      wire(svg, MODULE_PORT_X, yA, ESP_PORT_X, ep[0]._y, C.orange, 'نارنجی');
      wire(svg, MODULE_PORT_X, yV, ESP_PORT_X, ep[1]._y, C.cyan, '3.3V');
      wire(svg, MODULE_PORT_X, yG, ESP_PORT_X, ep[2]._y, C.black, 'GND');
    },
  });

  // ─── 5. DHT11 ───
  createCard({
    title: '۵. DHT11 (دما و رطوبت هوا)',
    color: '#58a6ff',
    desc: 'سنسور محیط — یک پین داده دیجیتال.',
    rows: [
      ['VCC', 'آبی روشن', 'ESP 3.3V'],
      ['GND', 'مشکی', 'GND مشترک'],
      ['DATA', 'نارنجی', 'ESP GPIO 4'],
    ],
    draw(svg) {
      moduleBox(svg, 40, 130, 'DHT11 ماژول', '#58a6ff');
      const ep = espBox(svg, 40, 100, [
        { label: 'GPIO 4', hint: 'DATA', color: C.orange },
        { label: '3.3V', color: C.cyan },
        { label: 'GND', color: C.black },
      ]);
      modulePin(svg, 80, 'VCC', C.cyan);
      modulePin(svg, 110, 'GND', C.black);
      modulePin(svg, 140, 'DATA', C.orange);
      wire(svg, MODULE_PORT_X, 140, ESP_PORT_X, ep[0]._y, C.orange, 'نارنجی');
      wire(svg, MODULE_PORT_X, 80, ESP_PORT_X, ep[1]._y, C.cyan, '3.3V');
      wire(svg, MODULE_PORT_X, 110, ESP_PORT_X, ep[2]._y, C.black, 'GND');
    },
  });

  // ─── 6. AP3216 ───
  createCard({
    title: '۶. AP3216 (نور محیط)',
    color: '#a371f7',
    desc: 'I2C — دو پین داده SDA و SCL.',
    rows: [
      ['VCC', 'آبی روشن', 'ESP 3.3V'],
      ['GND', 'مشکی', 'GND مشترک'],
      ['SDA', 'آبی', 'ESP GPIO 5'],
      ['SCL', 'بنفش', 'ESP GPIO 9'],
    ],
    draw(svg) {
      moduleBox(svg, 30, 170, 'AP3216', '#a371f7', 'I2C');
      const ep = espBox(svg, 30, 170, [
        { label: 'GPIO 5 (SDA)', color: C.blue },
        { label: 'GPIO 9 (SCL)', color: C.purple },
        { label: '3.3V', color: C.cyan },
        { label: 'GND', color: C.black },
      ]);
      modulePin(svg, 70, 'VCC', C.cyan);
      modulePin(svg, 100, 'GND', C.black);
      modulePin(svg, 130, 'SDA', C.blue);
      modulePin(svg, 160, 'SCL', C.purple);
      wire(svg, MODULE_PORT_X, 130, ESP_PORT_X, ep[0]._y, C.blue, 'آبی');
      wire(svg, MODULE_PORT_X, 160, ESP_PORT_X, ep[1]._y, C.purple, 'بنفش');
      wire(svg, MODULE_PORT_X, 70, ESP_PORT_X, ep[2]._y, C.cyan, '3.3V');
      wire(svg, MODULE_PORT_X, 100, ESP_PORT_X, ep[3]._y, C.black, 'GND');
    },
  });

  // ─── 7. سطح آب ───
  createCard({
    title: '۷. سنسور سطح آب',
    color: '#58a6ff',
    desc: 'سه پین: + − S. وقتی آب به خط‌ها می‌رسد سیگنال HIGH.',
    rows: [
      ['+ (VCC)', 'آبی روشن', 'ESP 3.3V'],
      ['− (GND)', 'مشکی', 'GND مشترک'],
      ['S (Signal)', 'نارنجی', 'ESP GPIO 7'],
    ],
    draw(svg) {
      moduleBox(svg, 40, 130, 'سنسور سطح آب', '#58a6ff', 'خط‌های فلزی در مخزن');
      const ep = espBox(svg, 40, 100, [
        { label: 'GPIO 7', hint: 'Signal', color: C.orange },
        { label: '3.3V', color: C.cyan },
        { label: 'GND', color: C.black },
      ]);
      modulePin(svg, 80, '+ VCC', C.cyan);
      modulePin(svg, 110, '− GND', C.black);
      modulePin(svg, 140, 'S', C.orange);
      wire(svg, MODULE_PORT_X, 140, ESP_PORT_X, ep[0]._y, C.orange, 'نارنجی');
      wire(svg, MODULE_PORT_X, 80, ESP_PORT_X, ep[1]._y, C.cyan, '3.3V');
      wire(svg, MODULE_PORT_X, 110, ESP_PORT_X, ep[2]._y, C.black, 'GND');
    },
  });

  // ─── 8. ULN2003 + رله + پمپ ───
  createCard({
    title: '۸. ULN2003AN + رله + پمپ 3V (×2)',
    color: C.gold,
    desc: 'مسیر: GPIO → ULN → IN رله → COM/NO → پمپ. رله LOW-trigger.',
    height: 380,
    rows: [
      ['ULN 1B', 'نارنجی', 'GPIO 10'],
      ['ULN 1C', 'نارنجی', 'IN رله ۱'],
      ['ULN 2B', 'نارنجی', 'GPIO 11'],
      ['ULN 2C', 'نارنجی', 'IN رله ۲'],
      ['ULN 8', 'مشکی', 'GND'],
      ['ULN 9 COM', 'قرمز', '5V'],
      ['رله VCC', 'قرمز', '5V پاوربانک'],
      ['پمپ +', 'قرمز', 'NO رله ← COM منبع 3V'],
      ['پمپ −', 'مشکی', 'GND منبع 3V'],
    ],
    draw(svg) {
      // ESP
      svg.appendChild(el('rect', {
        x: 24, y: 30, width: 100, height: 100, rx: 6,
        fill: '#1a2332', stroke: '#58a6ff', 'stroke-width': 2,
      }));
      svg.appendChild(txt(74, 50, 'ESP32', { anchor: 'middle', fill: '#58a6ff', size: 10, bold: true }));
      const g10 = 70, g11 = 100;
      svg.appendChild(port(124, g10, C.orange));
      svg.appendChild(txt(24, 74, 'GPIO 10', { fill: C.orange, size: 9 }));
      svg.appendChild(port(124, g11, C.orange));
      svg.appendChild(txt(24, 104, 'GPIO 11', { fill: C.orange, size: 9 }));

      // ULN
      svg.appendChild(el('rect', {
        x: 180, y: 40, width: 130, height: 200, rx: 6,
        fill: C.box, stroke: C.gold, 'stroke-width': 2,
      }));
      svg.appendChild(txt(235, 58, 'ULN2003AN', { anchor: 'middle', fill: C.gold, size: 11, bold: true }));
      const ulnOutX = 310;
      const u1b = 80, u2b = 110, u1c = 80, u2c = 110, ug = 220, ucom = 250;
      svg.appendChild(port(180, u1b, C.orange));
      svg.appendChild(txt(188, u1b + 4, '1B', { size: 9 }));
      svg.appendChild(port(180, u2b, C.orange));
      svg.appendChild(txt(188, u2b + 4, '2B', { size: 9 }));
      svg.appendChild(port(ulnOutX, u1c, C.orange));
      svg.appendChild(txt(ulnOutX + 8, u1c + 4, '1C', { size: 9 }));
      svg.appendChild(port(ulnOutX, u2c, C.orange));
      svg.appendChild(txt(ulnOutX + 8, u2c + 4, '2C', { size: 9 }));
      svg.appendChild(port(180, ug, C.black));
      svg.appendChild(txt(188, ug + 4, '8 GND', { size: 9 }));
      svg.appendChild(port(180, ucom, C.red));
      svg.appendChild(txt(188, ucom + 4, '9 COM', { size: 9 }));

      wireDirect(svg, 124, g10, 180, u1b, C.orange);
      wireDirect(svg, 124, g11, 180, u2b, C.orange);

      // Relay 1
      svg.appendChild(el('rect', {
        x: 380, y: 50, width: 110, height: 90, rx: 6,
        fill: C.box, stroke: '#3fb950', 'stroke-width': 2,
      }));
      svg.appendChild(txt(435, 68, 'رله ۱', { anchor: 'middle', fill: '#3fb950', size: 10, bold: true }));
      const r1in = 95;
      svg.appendChild(port(380, r1in, C.orange));
      svg.appendChild(txt(388, r1in + 4, 'IN', { size: 9 }));
      wireDirect(svg, ulnOutX, u1c, 380, r1in, C.orange);

      // Relay 2
      svg.appendChild(el('rect', {
        x: 380, y: 160, width: 110, height: 70, rx: 6,
        fill: C.box, stroke: '#3fb950', 'stroke-width': 2,
      }));
      svg.appendChild(txt(435, 178, 'رله ۲', { anchor: 'middle', fill: '#3fb950', size: 10, bold: true }));
      const r2in = 195;
      svg.appendChild(port(380, r2in, C.orange));
      wireDirect(svg, ulnOutX, u2c, 380, r2in, C.orange);

      // Pumps
      svg.appendChild(el('rect', {
        x: 540, y: 55, width: 90, height: 45, rx: 5,
        fill: C.box, stroke: '#d4a72c', 'stroke-width': 2,
      }));
      svg.appendChild(txt(585, 80, 'پمپ 3V ۱', { anchor: 'middle', fill: '#d4a72c', size: 9 }));
      svg.appendChild(el('rect', {
        x: 540, y: 165, width: 90, height: 45, rx: 5,
        fill: C.box, stroke: '#d4a72c', 'stroke-width': 2,
      }));
      svg.appendChild(txt(585, 190, 'پمپ 3V ۲', { anchor: 'middle', fill: '#d4a72c', size: 9 }));
      wireDirect(svg, 490, 75, 540, 75, C.red, 'NO');
      wireDirect(svg, 490, 185, 540, 185, C.red, 'NO');

      // 5V / GND buses
      busLine(svg, 28, C.red, '+5V', 180, 680);
      wireDirect(svg, 180, ucom, 400, 28, C.red);
      wireDirect(svg, 380, 70, 400, 28, C.red);
      wireDirect(svg, 380, 180, 400, 28, C.red);
      busLine(svg, 330, C.black, 'GND', 180, 620);
      wireDirect(svg, 180, ug, 400, 330, C.black);

      // 3V source
      svg.appendChild(el('rect', {
        x: 540, y: 280, width: 120, height: 50, rx: 5,
        fill: C.box, stroke: '#d4a72c', 'stroke-width': 2,
      }));
      svg.appendChild(txt(600, 305, 'منبع 3V', { anchor: 'middle', fill: '#d4a72c', size: 10, bold: true }));
      wireDirect(svg, 600, 280, 490, 75, C.red, 'COM');
      wireDirect(svg, 600, 330, 585, 100, C.black, '−');

      svg.appendChild(txt(360, 360, 'digitalWrite(10,HIGH) = پمپ ۱ روشن', {
        anchor: 'middle', fill: C.gold, size: 10,
      }));
    },
  });
})();
