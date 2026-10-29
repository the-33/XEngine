'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const {
  DEFAULT_SITE, parseIndex, parseLocalIndex, localCommandLink, chooseProfile, scopeBefore,
  findLinks, formatHover, normalizeBaseUrl,
} = require('../index');

const FIXTURE = `<?xml version="1.0"?>
<doxygenindex version="1.18.0">
<compound refid="classGameObject" kind="class"><name>GameObject</name>
<member refid="classGameObject_1a1b2c" kind="function"><name>SetActive</name></member>
</compound>
<compound refid="classAssetManager" kind="class"><name>AssetManager</name>
<member refid="classAssetManager_1ae2" kind="function"><name>LoadTexture</name></member>
<member refid="classAssetManager_1ae3" kind="function"><name>LoadTexture</name></member>
</compound>
<compound refid="classPlayer" kind="class"><name>Player</name>
<member refid="classPlayer_1a6" kind="function"><name>Update</name></member>
</compound>
<compound refid="classScene" kind="class"><name>Scene</name>
<member refid="classScene_1a4" kind="function"><name>Update</name></member>
</compound></doxygenindex>`;

test('parses precise class and member HTML anchors from Doxygen XML', () => {
  const index = parseIndex(FIXTURE, 'game');
  assert.equal(findLinks(index, 'GameObject')[0].url,
    `${DEFAULT_SITE}game/html/classGameObject.html`);
  assert.equal(findLinks(index, 'SetActive', { scope: 'GameObject' })[0].url,
    `${DEFAULT_SITE}game/html/classGameObject.html#a1b2c`);
  assert.equal(findLinks(index, 'LoadTexture', { scope: 'AssetManager' }).length, 2);
});

test('local XML creates native file URLs and an allowlisted command link', () => {
  const local = parseLocalIndex(FIXTURE, 'game', '/some/XEngine/build/docs/game/html');
  const memberUrl = findLinks(local, 'SetActive', { scope: 'GameObject' })[0].url;
  assert.equal(memberUrl,
    'file:///some/XEngine/build/docs/game/html/classGameObject.html#a1b2c');
  const remoteUrl = `${DEFAULT_SITE}game/html/classGameObject.html#a1b2c`;
  const command = localCommandLink(memberUrl, remoteUrl);
  assert.match(command, /^command:xengineDoxygen\.openLocal\?/);
  assert.deepEqual(JSON.parse(decodeURIComponent(command.split('?')[1])),
    [memberUrl, remoteUrl]);
  assert.throws(() => localCommandLink('https://other.invalid/file.html', remoteUrl),
    /local Doxygen/);
  assert.throws(() => localCommandLink(memberUrl, 'javascript:alert(1)'), /HTTPS/);
});

test('omits ambiguous unqualified methods when definition is unknown', () => {
  const index = parseIndex(FIXTURE, 'game');
  assert.deepEqual(findLinks(index, 'Update'), []);
  assert.deepEqual(findLinks(index, 'Update', {
    definitions: ['C:/games/My Game/Player.cpp'],
  }), []);
  assert.equal(findLinks(index, 'Update', { scope: 'Scene' })[0].owner, 'Scene');
});

test('resolves method ownership using C++ Go to Definition header', () => {
  const index = parseIndex(FIXTURE, 'game');
  assert.equal(findLinks(index, 'Update', {
    definitions: ['C:\\XEngine\\engine\\include\\Scene.h'],
  })[0].owner, 'Scene');
  assert.deepEqual(findLinks(index, 'Update', {
    definitions: ['C:\\project\\game\\src\\Update.cpp'],
  }), []);
});

test('honors URL validation, path profile detection and explicit C++ scope', () => {
  assert.equal(normalizeBaseUrl('https://the-33.github.io/XEngine'), DEFAULT_SITE);
  assert.throws(() => normalizeBaseUrl('javascript:alert(1)'), /HTTPS/);
  assert.equal(chooseProfile('C:\\XEngine\\engine\\src\\Scene.cpp'), 'engine');
  assert.equal(chooseProfile('/proj/XEngine/games/MyGame/main.cpp'), 'game');
  assert.equal(chooseProfile('/proj/XEngine/games/MyGame/main.cpp', 'engine'), 'engine');
  assert.equal(scopeBefore('   GameObject::SetActive()', 15), 'GameObject');
});

test('produces clickable HTTPS links without command URLs or raw HTML', () => {
  const index = parseIndex(FIXTURE, 'game');
  const value = formatHover(findLinks(index, 'GameObject'));
  assert.match(value, /\[📖 Open in XEngine documentation\]\(https:\/\/the-33\.github\.io/);
});

test('reads actual XEngine generated XML when it is available', () => {
  const root = path.join(__dirname, '..', '..', 'XEngine-v0.1.0');
  const xmlPath = path.join(root, 'build', 'docs', 'game', 'xml', 'index.xml');
  if (!fs.existsSync(xmlPath)) return;
  const index = parseIndex(fs.readFileSync(xmlPath, 'utf8'), 'game');
  const member = findLinks(index, 'LoadTexture', { scope: 'AssetManager' });
  assert.ok(member.length >= 1);
  assert.match(member[0].url, /game\/html\/classAssetManager\.html#a/);
  const htmlPath = path.join(root, 'build', 'docs', 'game', 'html', 'classAssetManager.html');
  if (fs.existsSync(htmlPath)) {
    const anchor = new URL(member[0].url).hash.slice(1);
    assert.ok(fs.readFileSync(htmlPath, 'utf8').includes(`href="#${anchor}"`),
      `Generated Doxygen HTML does not contain member anchor ${anchor}`);
  }
});
