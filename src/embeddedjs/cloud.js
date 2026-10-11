function randomGenerator(seed) {
	if (seed === undefined || seed === null) return Math.random;
	let state = Number(seed) >>> 0;
	return function () {
		state = (state + 0x6d2b79f5) >>> 0;
		let value = Math.imul(state ^ (state >>> 15), 1 | state);
		value ^= value + Math.imul(value ^ (value >>> 7), 61 | value);
		return ((value ^ (value >>> 14)) >>> 0) / 4294967296;
	};
}

function randomBetween(random, minimum, maximum) {
	return minimum + random() * (maximum - minimum);
}

export function verticalShapeInfluence(originY, floor, height, radiusY,
	lateralness, energy, midpoint, flatteningStrength, upperLiftStrength) {
	const fraction = Math.min(1, Math.max(0, (floor - originY) / Math.max(1, height)));
	const lower = Math.max(0, (midpoint - fraction) / midpoint);
	const upper = Math.max(0, (fraction - midpoint) / Math.max(0.001, 1 - midpoint));
	return radiusY * energy *
		(flatteningStrength * Math.pow(lateralness, 4) * lower - upperLiftStrength * upper);
}

export function fractalCloudLobes(width, height, floor, rootCenterX, options = {}) {
	const size = options.sizeMultiplier === undefined ? 1 : options.sizeMultiplier;
	const random = randomGenerator(options.seed);
	const radius = Math.round(Math.min(width, height) * 0.25 * size);
	const root = {
		centerX: Math.round(rootCenterX == null ? width * 0.48 : rootCenterX),
		centerY: floor - radius, radiusX: radius, radiusY: radius,
		depth: 0, heading: -1.57,
	};
	const lobes = [root];
	let frontier = [root];
	const rootHeadings = [-1.57, -0.12, 3.02, -2.26, -0.78];
	for (let depth = 1; depth <= 4; depth += 1) {
		const next = [];
		for (const parent of frontier) {
			const headings = depth === 1 ? rootHeadings : [];
			if (depth !== 1) {
				for (let branch = 0; branch < 5; branch += 1) {
					headings.push(randomBetween(random, parent.heading - 0.64, parent.heading + 0.64));
				}
			}
			for (const direction of headings) {
				const turbulence = randomBetween(random, -1, 1);
				const scale = randomBetween(random, 0.39, 0.53);
				const originX = depth === 1 ? root.centerX : parent.centerX;
				const originY = depth === 1 ? floor : parent.centerY;
				const cosine = Math.cos(direction);
				const upwardness = Math.max(0, -Math.sin(direction));
				const projectedX = originX + cosine * parent.radiusX * 0.6;
				const alignment = Math.max(0, 1 - Math.abs(projectedX - root.centerX) / (width * 0.38));
				const velocity = 0.32 + 0.68 * alignment * upwardness * (depth === 1 ? 0.55 : 1);
				const branchScale = (0.32 + 0.4 * velocity) * scale * 1.7 * 1.42 * size;
				const radiusX = Math.max(8, Math.round(parent.radiusX * branchScale));
				const radiusY = Math.max(7, Math.round(parent.radiusY * branchScale));
				const lateralness = Math.abs(cosine);
				const reach = parent.radiusX * (0.55 + 0.66 * lateralness + turbulence * 0.06) * 1.42;
				const lift = parent.radiusY * velocity * (0.72 + turbulence * 0.05) * (1.15 + 0.55 * upwardness);
				const settling = parent.radiusY * (1 - velocity) * (0.36 + turbulence * 0.04);
				const influence = verticalShapeInfluence(originY, floor, height, parent.radiusY,
					lateralness, 1.42, 0.5, 0.58, 0.18);
				const child = {
					centerX: Math.max(radiusX, Math.round(originX + cosine * reach)),
					centerY: Math.min(floor - radiusY, Math.round(originY - lift + settling + influence)),
					radiusX, radiusY, depth, heading: direction,
				};
				lobes.push(child);
				next.push(child);
			}
		}
		frontier = next;
	}
	return lobes;
}

function maskHas(mask, index) {
	return (mask[index >> 3] & (1 << (index & 7))) !== 0;
}

function maskSet(mask, index) {
	mask[index >> 3] |= 1 << (index & 7);
}

function ellipsePixels(width, height, lobe, visit) {
	const top = Math.max(0, Math.ceil(lobe.centerY - lobe.radiusY - 0.5));
	const bottom = Math.min(height - 1, Math.floor(lobe.centerY + lobe.radiusY - 0.5));
	for (let row = top; row <= bottom; row += 1) {
		const relativeY = (row + 0.5 - lobe.centerY) / lobe.radiusY;
		const reach = lobe.radiusX * Math.sqrt(Math.max(0, 1 - relativeY * relativeY));
		const left = Math.max(0, Math.ceil(lobe.centerX - reach - 0.5));
		const right = Math.min(width - 1, Math.floor(lobe.centerX + reach - 0.5));
		for (let column = left; column <= right; column += 1) {
			visit(row * width + column, column, row);
		}
	}
}

export function createCloud(width, height, options = {}) {
	width = Math.floor(width);
	height = Math.floor(height);
	if (!Number.isFinite(width) || !Number.isFinite(height) || width < 1 || height < 1) {
		throw new RangeError("Cloud dimensions must be positive and finite");
	}
	if (options.sizeMultiplier !== undefined &&
		(!Number.isFinite(options.sizeMultiplier) || options.sizeMultiplier <= 0)) {
		throw new RangeError("Cloud sizeMultiplier must be positive and finite");
	}
	const floor = options.floor === undefined ? Math.round(height * 0.98) : options.floor;
	const rootCenterX = options.rootCenterX === undefined ? Math.round(width * 0.48) : options.rootCenterX;
	if (!Number.isFinite(floor) || !Number.isFinite(rootCenterX)) {
		throw new RangeError("Cloud floor and rootCenterX must be finite");
	}
	const lobes = fractalCloudLobes(width, height, floor, rootCenterX, options);
	const contours = fractalCloudLobes(width, height, floor, rootCenterX,
		{ seed: options.seed, sizeMultiplier: 0.92 });
	let left = Infinity;
	let top = floor - 38;
	let right = -Infinity;
	for (const collection of [lobes, contours]) {
		for (const lobe of collection) {
			left = Math.min(left, lobe.centerX - lobe.radiusX);
			top = Math.min(top, lobe.centerY - lobe.radiusY);
			right = Math.max(right, lobe.centerX + lobe.radiusX);
		}
	}
	const margin = Math.min(4, (width - 1) / 2, (height - 1) / 2);
	const fit = Math.min(1, (width - 2 * margin) / (right - left),
		(height - 2 * margin) / (floor - top));
	const offsetX = (width - (right - left) * fit) / 2;
	const offsetY = height - margin - (floor - top) * fit;
	for (const collection of [lobes, contours]) {
		for (const lobe of collection) {
			lobe.centerX = offsetX + (lobe.centerX - left) * fit;
			lobe.centerY = offsetY + (lobe.centerY - top) * fit;
			lobe.radiusX *= fit;
			lobe.radiusY *= fit;
		}
	}
	const fittedFloor = height - margin;
	const pixels = new Uint8Array(Math.ceil(width * height / 2));
	const silhouette = new Uint8Array(Math.ceil(width * height / 8));
	const contourMask = new Uint8Array(silhouette.length);
	function setPixel(index, color) {
		const shift = (index & 1) * 4;
		pixels[index >> 1] = (pixels[index >> 1] & ~(15 << shift)) | (color << shift);
	}
	function getPixel(index) {
		return (pixels[index >> 1] >> ((index & 1) * 4)) & 15;
	}
	const root = lobes[0];
	const baseTop = fittedFloor - 38 * fit;
	const baseLeft = root.centerX - root.radiusX;
	const baseRight = root.centerX + root.radiusX;
	const corner = Math.min(18 * fit, root.radiusX, 19 * fit);
	for (let row = Math.max(0, Math.floor(baseTop)); row < Math.min(height, Math.ceil(fittedFloor)); row += 1) {
		for (let column = Math.max(0, Math.floor(baseLeft)); column < Math.min(width, Math.ceil(baseRight)); column += 1) {
			const sampleX = column + 0.5;
			const sampleY = row + 0.5;
			if (sampleX < baseLeft || sampleX > baseRight || sampleY < baseTop || sampleY > fittedFloor) continue;
			const deltaX = Math.max(baseLeft + corner - sampleX, 0, sampleX - (baseRight - corner));
			const deltaY = Math.max(baseTop + corner - sampleY, 0, sampleY - (fittedFloor - corner));
			if (deltaX * deltaX + deltaY * deltaY <= corner * corner) maskSet(silhouette, row * width + column);
		}
	}
	for (const lobe of lobes) ellipsePixels(width, height, lobe, index => maskSet(silhouette, index));
	for (let index = 0; index < width * height; index += 1) {
		if (maskHas(silhouette, index)) setPixel(index, 1);
	}
	function paintLobe(lobe, shade, clipped) {
		const upperRadiusX = Math.max(1, Math.round(lobe.radiusX / fit * 1.08)) * fit;
		const upperRadiusY = Math.max(1, Math.round(lobe.radiusY / fit * 0.64)) * fit;
		const upperCenterY = lobe.centerY - lobe.radiusY + upperRadiusY;
		ellipsePixels(width, height, lobe, (index, column, row) => {
			if (clipped && !maskHas(silhouette, index)) return;
			if (shade) {
				const dot = (column % 4 === 0 && row % 4 === 0) || (column % 4 === 2 && row % 4 === 2);
				setPixel(index, dot ? 2 : 1);
			}
			const deltaX = (column + 0.5 - lobe.centerX) / upperRadiusX;
			const deltaY = (row + 0.5 - upperCenterY) / upperRadiusY;
			if (deltaX * deltaX + deltaY * deltaY <= 1) setPixel(index, 1);
		});
	}
	const random = randomGenerator(options.seed == null ? undefined : Number(options.seed) + 1);
	for (let depth = 4; depth >= 0; depth -= 1) {
		for (const lobe of lobes) {
			if (lobe.depth === depth) paintLobe(lobe, depth >= 3 && random() < 2 / 3, false);
		}
	}
	for (const lobe of contours) {
		if (lobe.depth === 3 && random() < 0.75 && lobe.centerY < fittedFloor - 20 * fit) {
			ellipsePixels(width, height, lobe, index => {
				maskSet(contourMask, index);
				setPixel(index, 1);
			});
		}
	}
	function outlineMask(mask, radius, color) {
		for (let row = 0; row < height; row += 1) {
			for (let column = 0; column < width; column += 1) {
				const index = row * width + column;
				if (!maskHas(mask, index)) continue;
				let edge = false;
				for (let sampleY = row - radius; sampleY <= row + radius && !edge; sampleY += 1) {
					for (let sampleX = column - radius; sampleX <= column + radius; sampleX += 1) {
						if (sampleX < 0 || sampleX >= width || sampleY < 0 || sampleY >= height ||
							!maskHas(mask, sampleY * width + sampleX)) {
							edge = true;
							break;
						}
					}
				}
				if (edge) setPixel(index, color);
			}
		}
	}
	outlineMask(contourMask, 1, 4);
	for (const lobe of contours) {
		if (lobe.depth === 2 && lobe.centerY < fittedFloor - 20 * fit) {
			ellipsePixels(width, height, lobe, index => {
				if (maskHas(silhouette, index)) setPixel(index, 1);
			});
		}
	}
	for (const lobe of contours) {
		if (lobe.depth === 2 && lobe.centerY < fittedFloor - 20 * fit && random() < 2 / 3) {
			paintLobe(lobe, true, true);
		}
	}
	for (let index = 0; index < silhouette.length; index += 1) silhouette[index] |= contourMask[index];
	outlineMask(silhouette, 2, 3);
	if (options.debug) {
		for (const lobe of lobes) {
			ellipsePixels(width, height, lobe, (index, column, row) => {
				const deltaX = (column + 0.5 - lobe.centerX) / Math.max(0.01, lobe.radiusX - 1);
				const deltaY = (row + 0.5 - lobe.centerY) / Math.max(0.01, lobe.radiusY - 1);
				if (deltaX * deltaX + deltaY * deltaY >= 1) setPixel(index, 3);
			});
		}
	}
	return {
		width, height, left: offsetX, top: offsetY,
		right: offsetX + (right - left) * fit, bottom: fittedFloor,
		draw(render, x = 0, y = 0) {
			x = Math.round(x);
			y = Math.round(y);
			if (!Number.isFinite(x) || !Number.isFinite(y) || x < 0 || y < 0 ||
				x + width > render.width || y + height > render.height) {
				throw new RangeError("Cloud must fit within the Poco renderer");
			}
			const palette = [0,
				options.cloudFill === undefined ? render.makeColor(255, 255, 255) : options.cloudFill,
				options.shadowColor === undefined ? render.makeColor(80, 80, 80) : options.shadowColor,
				options.cloudOutline === undefined ? render.makeColor(0, 0, 0) : options.cloudOutline,
				options.innerOutline === undefined ? render.makeColor(50, 50, 50) : options.innerOutline];
			for (let row = 0; row < height; row += 1) {
				let column = 0;
				while (column < width) {
					const start = column;
					const color = getPixel(row * width + column);
					while (++column < width && getPixel(row * width + column) === color) {}
					if (color) render.fillRectangle(palette[color], x + start, y + row, column - start, 1);
				}
			}
		},
	};
}

export function drawCloud(render, options = {}) {
	const x = options.x === undefined ? 0 : Math.round(options.x);
	const y = options.y === undefined ? 0 : Math.round(options.y);
	const cloud = createCloud(options.width === undefined ? render.width - x : options.width,
		options.height === undefined ? render.height - y : options.height, options);
	cloud.draw(render, x, y);
	return cloud;
}

export default { drawCloud, createCloud, fractalCloudLobes, verticalShapeInfluence };
