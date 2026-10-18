'use strict';

var messageKeys = require('message_keys');
var configUrl = 'https://a-type.github.io/nimbus-watchface/config.html';

Pebble.addEventListener('showConfiguration', function () {
	var analogMode = localStorage.getItem('analog_mode') === 'true';
	Pebble.openURL(configUrl + '?analog_mode=' + (analogMode ? '1' : '0'));
});

Pebble.addEventListener('webviewclosed', function (event) {
	if (!event || !event.response) return;

	var configData;
	try {
		configData = JSON.parse(decodeURIComponent(event.response));
	} catch (error) {
		console.log('Invalid watchface settings response');
		return;
	}
	if (typeof configData.analog_mode !== 'boolean') return;

	localStorage.setItem(
		'analog_mode',
		configData.analog_mode ? 'true' : 'false'
	);
	var message = {};
	message[messageKeys.ANALOG_MODE] = configData.analog_mode ? 1 : 0;
	var onMessageSent = function () {
		console.log('Watchface settings sent');
	};
	var onMessageFailed = function (error) {
		console.log('Failed to send watchface settings: ' + JSON.stringify(error));
	};
	Pebble.sendAppMessage(message, onMessageSent, onMessageFailed);
});
