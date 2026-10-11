import Poco from "commodetto/Poco";
import { createCloud } from "cloud";

console.log("Hello, Watchface.");

let render = new Poco(screen);
const cloud = createCloud(render.width, render.height, { seed: 42 });

const font = new render.Font("Bitham-Black", 30);
const black = render.makeColor(0, 0, 0);
const white = render.makeColor(255, 255, 255);

function draw() {
	render.begin();
	render.fillRectangle(white, 0, 0, render.width, render.height);
	cloud.draw(render);
	
	const msg = (new Date).toTimeString().slice(0, 8);
	const width = render.getTextWidth(msg, font);

	render.drawText(msg, font, black,
		(render.width - width) / 2, (render.height - font.height) / 2);
 
	render.end();
}

watch.addEventListener('secondchange', draw);
