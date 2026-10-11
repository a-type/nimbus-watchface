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

const LOBE_DEPTH_STARTS = [0, 1, 6, 31];
const LOBE_DEPTH_ENDS = [1, 6, 31, 156];

export function verticalShapeInfluence(
	originY,
	floor,
	height,
	radiusY,
	lateralness,
	energy,
	midpoint,
	flatteningStrength,
	upperLiftStrength,
) {
	const fraction = Math.min(
		1,
		Math.max(0, (floor - originY) / Math.max(1, height)),
	);
	const lower = Math.max(0, (midpoint - fraction) / midpoint);
	const upper = Math.max(
		0,
		(fraction - midpoint) / Math.max(0.001, 1 - midpoint),
	);
	return (
		radiusY *
		energy *
		(flatteningStrength * Math.pow(lateralness, 4) * lower -
			upperLiftStrength * upper)
	);
}

function generateCloudLobeData(
	width,
	height,
	floor,
	rootCenterX,
	options = {},
) {
	const size =
		options.sizeMultiplier === undefined ? 1 : options.sizeMultiplier;
	const random = randomGenerator(options.seed);
	const radius = Math.round(Math.min(width, height) * 0.25 * size);
	const positions = new Float32Array(LOBE_DEPTH_ENDS[3] * 4);
	const headings = new Float64Array(LOBE_DEPTH_ENDS[3]);
	positions[0] = Math.round(rootCenterX == null ? width * 0.48 : rootCenterX);
	positions[1] = floor - radius;
	positions[2] = radius;
	positions[3] = radius;
	headings[0] = -1.57;
	const rootHeadings = [-1.57, -0.12, 3.02, -2.26, -0.78];
	const branchHeadings = new Array(5);
	for (let depth = 1; depth <= 3; depth += 1) {
		let childIndex = LOBE_DEPTH_STARTS[depth];
		for (
			let parentIndex = LOBE_DEPTH_STARTS[depth - 1];
			parentIndex < LOBE_DEPTH_ENDS[depth - 1];
			parentIndex += 1
		) {
			const parentOffset = parentIndex * 4;
			const parentX = positions[parentOffset];
			const parentY = positions[parentOffset + 1];
			const parentRadiusX = positions[parentOffset + 2];
			const parentRadiusY = positions[parentOffset + 3];
			if (depth !== 1) {
				for (let branch = 0; branch < 5; branch += 1) {
					branchHeadings[branch] = randomBetween(
						random,
						headings[parentIndex] - 0.64,
						headings[parentIndex] + 0.64,
					);
				}
			}
			for (let branch = 0; branch < 5; branch += 1) {
				const direction =
					depth === 1 ? rootHeadings[branch] : branchHeadings[branch];
				const turbulence = randomBetween(random, -1, 1);
				const scale = randomBetween(random, 0.39, 0.53);
				const originX = depth === 1 ? positions[0] : parentX;
				const originY = depth === 1 ? floor : parentY;
				const cosine = Math.cos(direction);
				const upwardness = Math.max(0, -Math.sin(direction));
				const projectedX = originX + cosine * parentRadiusX * 0.6;
				const alignment = Math.max(
					0,
					1 - Math.abs(projectedX - positions[0]) / (width * 0.38),
				);
				const velocity =
					0.32 + 0.68 * alignment * upwardness * (depth === 1 ? 0.55 : 1);
				const branchScale = (0.32 + 0.4 * velocity) * scale * 1.7 * 1.42 * size;
				const radiusX = Math.max(8, Math.round(parentRadiusX * branchScale));
				const radiusY = Math.max(7, Math.round(parentRadiusY * branchScale));
				const lateralness = Math.abs(cosine);
				const reach =
					parentRadiusX *
					(0.55 + 0.66 * lateralness + turbulence * 0.06) *
					1.42;
				const lift =
					parentRadiusY *
					velocity *
					(0.72 + turbulence * 0.05) *
					(1.15 + 0.55 * upwardness);
				const settling =
					parentRadiusY * (1 - velocity) * (0.36 + turbulence * 0.04);
				const influence = verticalShapeInfluence(
					originY,
					floor,
					height,
					parentRadiusY,
					lateralness,
					1.42,
					0.5,
					0.58,
					0.18,
				);
				const childOffset = childIndex * 4;
				positions[childOffset] = Math.max(
					radiusX,
					Math.round(originX + cosine * reach),
				);
				positions[childOffset + 1] = Math.min(
					floor - radiusY,
					Math.round(originY - lift + settling + influence),
				);
				positions[childOffset + 2] = radiusX;
				positions[childOffset + 3] = radiusY;
				headings[childIndex] = direction;
				childIndex += 1;
			}
		}
	}
	return { positions, headings };
}

export function fractalCloudLobes(
	width,
	height,
	floor,
	rootCenterX,
	options = {},
) {
	const data = generateCloudLobeData(
		width,
		height,
		floor,
		rootCenterX,
		options,
	);
	const lobes = new Array(LOBE_DEPTH_ENDS[3]);
	for (let depth = 0; depth <= 3; depth += 1) {
		for (
			let index = LOBE_DEPTH_STARTS[depth];
			index < LOBE_DEPTH_ENDS[depth];
			index += 1
		) {
			const offset = index * 4;
			lobes[index] = {
				centerX: data.positions[offset],
				centerY: data.positions[offset + 1],
				radiusX: data.positions[offset + 2],
				radiusY: data.positions[offset + 3],
				depth,
				heading: data.headings[index],
			};
		}
	}
	return lobes;
}

function ellipsePixels(
	width,
	height,
	centerX,
	centerY,
	radiusX,
	radiusY,
	visit,
) {
	const top = Math.max(0, Math.ceil(centerY - radiusY - 0.5));
	const bottom = Math.min(height - 1, Math.floor(centerY + radiusY - 0.5));
	for (let row = top; row <= bottom; row += 1) {
		const relativeY = (row + 0.5 - centerY) / radiusY;
		const reach = radiusX * Math.sqrt(Math.max(0, 1 - relativeY * relativeY));
		const left = Math.max(0, Math.ceil(centerX - reach - 0.5));
		const right = Math.min(width - 1, Math.floor(centerX + reach - 0.5));
		for (let column = left; column <= right; column += 1) {
			visit(row * width + column, column, row);
		}
	}
}

export function createCloud(width, height, options = {}) {
	width = Math.floor(width);
	height = Math.floor(height);
	if (
		!Number.isFinite(width) ||
		!Number.isFinite(height) ||
		width < 1 ||
		height < 1
	) {
		throw new RangeError('Cloud dimensions must be positive and finite');
	}
	if (
		options.sizeMultiplier !== undefined &&
		(!Number.isFinite(options.sizeMultiplier) || options.sizeMultiplier <= 0)
	) {
		throw new RangeError('Cloud sizeMultiplier must be positive and finite');
	}
	const floor =
		options.floor === undefined ? Math.round(height * 0.98) : options.floor;
	const rootCenterX =
		options.rootCenterX === undefined ?
			Math.round(width * 0.48)
		:	options.rootCenterX;
	if (!Number.isFinite(floor) || !Number.isFinite(rootCenterX)) {
		throw new RangeError('Cloud floor and rootCenterX must be finite');
	}
	const positions = generateCloudLobeData(
		width,
		height,
		floor,
		rootCenterX,
		options,
	).positions;
	const lobeCount = LOBE_DEPTH_ENDS[3];
	let left = Infinity;
	let top = floor - 38;
	let right = -Infinity;
	for (let index = 0; index < lobeCount; index += 1) {
		const offset = index * 4;
		left = Math.min(left, positions[offset] - positions[offset + 2]);
		top = Math.min(top, positions[offset + 1] - positions[offset + 3]);
		right = Math.max(right, positions[offset] + positions[offset + 2]);
	}
	const margin = Math.min(4, (width - 1) / 2, (height - 1) / 2);
	const fit = Math.min(
		1,
		(width - 2 * margin) / (right - left),
		(height - 2 * margin) / (floor - top),
	);
	const offsetX = (width - (right - left) * fit) / 2;
	const offsetY = height - margin - (floor - top) * fit;
	for (let index = 0; index < lobeCount; index += 1) {
		const offset = index * 4;
		positions[offset] = offsetX + (positions[offset] - left) * fit;
		positions[offset + 1] = offsetY + (positions[offset + 1] - top) * fit;
		positions[offset + 2] *= fit;
		positions[offset + 3] *= fit;
	}
	const fittedFloor = height - margin;
	const pixels = new Uint8Array(Math.ceil((width * height) / 4));
	function setPixel(index, color) {
		const shift = (index & 3) * 2;
		pixels[index >> 2] =
			(pixels[index >> 2] & ~(3 << shift)) | (color << shift);
	}
	function getPixel(index) {
		return (pixels[index >> 2] >> ((index & 3) * 2)) & 3;
	}
	const baseTop = fittedFloor - 38 * fit;
	const baseLeft = positions[0] - positions[2];
	const baseRight = positions[0] + positions[2];
	const corner = Math.min(18 * fit, positions[2], 19 * fit);
	for (
		let row = Math.max(0, Math.floor(baseTop));
		row < Math.min(height, Math.ceil(fittedFloor));
		row += 1
	) {
		for (
			let column = Math.max(0, Math.floor(baseLeft));
			column < Math.min(width, Math.ceil(baseRight));
			column += 1
		) {
			const sampleX = column + 0.5;
			const sampleY = row + 0.5;
			if (
				sampleX < baseLeft ||
				sampleX > baseRight ||
				sampleY < baseTop ||
				sampleY > fittedFloor
			)
				continue;
			const deltaX = Math.max(
				baseLeft + corner - sampleX,
				0,
				sampleX - (baseRight - corner),
			);
			const deltaY = Math.max(
				baseTop + corner - sampleY,
				0,
				sampleY - (fittedFloor - corner),
			);
			if (deltaX * deltaX + deltaY * deltaY <= corner * corner)
				setPixel(row * width + column, 1);
		}
	}
	for (let index = 0; index < lobeCount; index += 1) {
		const offset = index * 4;
		ellipsePixels(
			width,
			height,
			positions[offset],
			positions[offset + 1],
			positions[offset + 2],
			positions[offset + 3],
			(pixel) => setPixel(pixel, 1),
		);
	}
	function paintLobe(lobeIndex, shade) {
		const offset = lobeIndex * 4;
		const centerX = positions[offset];
		const centerY = positions[offset + 1];
		const radiusX = positions[offset + 2];
		const radiusY = positions[offset + 3];
		const upperRadiusX = Math.max(1, Math.round((radiusX / fit) * 1.08)) * fit;
		const upperRadiusY = Math.max(1, Math.round((radiusY / fit) * 0.64)) * fit;
		const upperCenterY = centerY - radiusY + upperRadiusY;
		ellipsePixels(
			width,
			height,
			centerX,
			centerY,
			radiusX,
			radiusY,
			(index, column, row) => {
				if (shade) {
					const dot =
						(column % 4 === 0 && row % 4 === 0) ||
						(column % 4 === 2 && row % 4 === 2);
					setPixel(index, dot ? 2 : 1);
				}
				const deltaX = (column + 0.5 - centerX) / upperRadiusX;
				const deltaY = (row + 0.5 - upperCenterY) / upperRadiusY;
				if (deltaX * deltaX + deltaY * deltaY <= 1) setPixel(index, 1);
			},
		);
	}
	const random = randomGenerator(
		options.seed == null ? undefined : Number(options.seed) + 1,
	);
	for (let depth = 3; depth >= 0; depth -= 1) {
		for (
			let index = LOBE_DEPTH_STARTS[depth];
			index < LOBE_DEPTH_ENDS[depth];
			index += 1
		) {
			paintLobe(index, depth >= 3 && random() < 2 / 3);
		}
	}
	const outlineLeft = Math.max(0, Math.floor(offsetX) - 2);
	const outlineRight = Math.min(
		width,
		Math.ceil(offsetX + (right - left) * fit) + 2,
	);
	const outlineTop = Math.max(0, Math.floor(offsetY) - 2);
	const outlineBottom = Math.min(height, Math.ceil(fittedFloor) + 1);
	function outlineSurface(radius) {
		for (let row = outlineTop; row < outlineBottom; row += 1) {
			for (let column = outlineLeft; column < outlineRight; column += 1) {
				const index = row * width + column;
				if (!getPixel(index)) continue;
				let edge = false;
				for (
					let sampleY = row - radius;
					sampleY <= row + radius && !edge;
					sampleY += 1
				) {
					for (
						let sampleX = column - radius;
						sampleX <= column + radius;
						sampleX += 1
					) {
						if (
							sampleX < 0 ||
							sampleX >= width ||
							sampleY < 0 ||
							sampleY >= height ||
							!getPixel(sampleY * width + sampleX)
						) {
							edge = true;
							break;
						}
					}
				}
				if (edge) setPixel(index, 3);
			}
		}
	}
	outlineSurface(2);
	if (options.debug) {
		for (let index = 0; index < lobeCount; index += 1) {
			const offset = index * 4;
			const centerX = positions[offset];
			const centerY = positions[offset + 1];
			const radiusX = positions[offset + 2];
			const radiusY = positions[offset + 3];
			ellipsePixels(
				width,
				height,
				centerX,
				centerY,
				radiusX,
				radiusY,
				(index, column, row) => {
					const deltaX = (column + 0.5 - centerX) / Math.max(0.01, radiusX - 1);
					const deltaY = (row + 0.5 - centerY) / Math.max(0.01, radiusY - 1);
					if (deltaX * deltaX + deltaY * deltaY >= 1) setPixel(index, 3);
				},
			);
		}
	}
	let palette;
	return {
		width,
		height,
		left: offsetX,
		top: offsetY,
		right: offsetX + (right - left) * fit,
		bottom: fittedFloor,
		draw(render, x = 0, y = 0) {
			x = Math.round(x);
			y = Math.round(y);
			if (
				!Number.isFinite(x) ||
				!Number.isFinite(y) ||
				x < 0 ||
				y < 0 ||
				x + width > render.width ||
				y + height > render.height
			) {
				throw new RangeError('Cloud must fit within the Poco renderer');
			}
			if (!palette) {
				palette = [
					0,
					options.cloudFill === undefined ?
						render.makeColor(255, 255, 255)
					:	options.cloudFill,
					options.shadowColor === undefined ?
						render.makeColor(80, 80, 80)
					:	options.shadowColor,
					options.cloudOutline === undefined ?
						render.makeColor(0, 0, 0)
					:	options.cloudOutline,
				];
			}
			for (let row = outlineTop; row < outlineBottom; row += 1) {
				let column = outlineLeft;
				while (column < outlineRight) {
					const start = column;
					const color = getPixel(row * width + column);
					while (
						++column < width &&
						getPixel(row * width + column) === color
					) {}
					if (color)
						render.fillRectangle(
							palette[color],
							x + start,
							y + row,
							column - start,
							1,
						);
				}
			}
		},
	};
}

export function drawCloud(render, options = {}) {
	const x = options.x === undefined ? 0 : Math.round(options.x);
	const y = options.y === undefined ? 0 : Math.round(options.y);
	const cloud = createCloud(
		options.width === undefined ? render.width - x : options.width,
		options.height === undefined ? render.height - y : options.height,
		options,
	);
	cloud.draw(render, x, y);
	return cloud;
}

export default {
	drawCloud,
	createCloud,
	fractalCloudLobes,
	verticalShapeInfluence,
};
