import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const source = await readFile(
	new URL('../src/embeddedjs/cloud.js', import.meta.url),
	'utf8',
);
const { createCloud, drawCloud, fractalCloudLobes } = await import(
	`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`
);

function renderer(width, height) {
	const calls = [];
	return {
		width,
		height,
		calls,
		makeColor(red, green, blue) {
			return (red << 16) | (green << 8) | blue;
		},
		fillRectangle(color, x, y, span, rows) {
			assert.ok(Number.isInteger(x) && Number.isInteger(y));
			assert.ok(x >= 0 && y >= 0 && span > 0 && rows > 0);
			assert.ok(x + span <= width && y + rows <= height);
			calls.push([color, x, y, span, rows]);
		},
	};
}

test('reference generation remains deterministic with three five-way generations', () => {
	const lobes = fractalCloudLobes(200, 228, 223, 96, { seed: 42 });
	assert.equal(lobes.length, 156);
	assert.deepEqual(lobes, fractalCloudLobes(200, 228, 223, 96, { seed: 42 }));
	assert.deepEqual(lobes[0], {
		centerX: 96,
		centerY: 173,
		radiusX: 50,
		radiusY: 50,
		depth: 0,
		heading: -1.57,
	});
});

test('clouds fit Pebble screens, narrow regions, and oversized source geometry', () => {
	for (const [width, height] of [
		[200, 228],
		[260, 260],
		[144, 168],
		[31, 97],
		[97, 31],
		[1, 1],
	]) {
		for (const sizeMultiplier of [0.5, 1, 2]) {
			const render = renderer(width, height);
			const cloud = drawCloud(render, { seed: 42, sizeMultiplier });
			assert.ok(cloud.left >= 0 && cloud.top >= 0);
			assert.ok(cloud.right <= width && cloud.bottom <= height);
			if (width > 1 && height > 1) assert.ok(render.calls.length > 0);
		}
	}
});

test('cached drawing is repeatable and supports an offset region and Poco colors', () => {
	const cloud = createCloud(100, 100, {
		seed: 7,
		cloudFill: 123,
		shadowColor: 456,
	});
	const first = renderer(200, 228);
	const second = renderer(200, 228);
	cloud.draw(first, 20, 30);
	cloud.draw(second, 20, 30);
	assert.deepEqual(first.calls, second.calls);
	assert.ok(first.calls.some((call) => call[0] === 123));
	assert.ok(first.calls.some((call) => call[0] === 456));
	assert.ok(first.calls.some((call) => call[0] === 0));
	assert.ok(!first.calls.some((call) => call[0] === 0x323232));
	assert.throws(() => cloud.draw(first, 150, 0), RangeError);
});

test('invalid dimensions and geometry fail explicitly', () => {
	for (const width of [0, -1, NaN, Infinity]) {
		assert.throws(() => createCloud(width, 100), RangeError);
	}
	for (const sizeMultiplier of [0, -1, NaN, Infinity]) {
		assert.throws(() => createCloud(100, 100, { sizeMultiplier }), RangeError);
	}
	assert.throws(() => createCloud(100, 100, { floor: NaN }), RangeError);
});
