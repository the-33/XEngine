'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const Module = require('node:module');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

const XML = '<doxygenindex><compound refid="classGameObject" kind="class">' +
  '<name>GameObject</name>' +
  '<member refid="classGameObject_1a123" kind="function"><name>SetActive</name></member>' +
  '</compound></doxygenindex>';

test('registered C++ hover provider adds a real Doxygen member link', async () => {
  let provider;
  const vscode = {
    languages: {
      registerHoverProvider: (_selector, value) => {
        provider = value;
        return { dispose() {} };
      },
    },
    workspace: {
      getConfiguration: () => ({
        get: (name, fallback) => name === 'preferLocal' ? false : fallback,
      }),
      onDidChangeConfiguration: () => ({ dispose() {} }),
    },
    window: {
      createOutputChannel: () => ({ appendLine() {}, dispose() {} }),
      showInformationMessage() {},
      showWarningMessage() {},
    },
    commands: {
      registerCommand: () => ({ dispose() {} }),
      executeCommand: async () => [
        { uri: { fsPath: 'C:\\XEngine\\engine\\include\\GameObject.h' } },
      ],
    },
    env: { openExternal: async () => true },
    Uri: { parse: input => input },
    MarkdownString: class {
      constructor(value) { this.value = value; }
    },
    Hover: class {
      constructor(contents, range) { this.contents = contents; this.range = range; }
    },
  };

  const originalLoad = Module._load;
  const originalFetch = global.fetch;
  try {
    Module._load = function (name, ...rest) {
      return name === 'vscode' ? vscode : originalLoad.call(this, name, ...rest);
    };
    global.fetch = async () => ({ ok: true, text: async () => XML });
    const { activate } = require('../extension');
    activate({ subscriptions: [] });
    assert.ok(provider);

    const range = { start: { character: 12 }, end: { character: 21 } };
    const document = {
      uri: { fsPath: 'C:\\XEngine\\games\\Sample\\src\\main.cpp' },
      getWordRangeAtPosition: () => range,
      getText: () => 'SetActive',
      lineAt: () => ({ text: 'GameObject::SetActive()' }),
    };
    const hover = await provider.provideHover(document, { line: 0, character: 14 },
      { isCancellationRequested: false });
    assert.match(hover.contents.value,
      /https:\/\/the-33\.github\.io\/XEngine\/game\/html\/classGameObject\.html#a123/);
    assert.equal(hover.range, range);
  } finally {
    Module._load = originalLoad;
    global.fetch = originalFetch;
    delete require.cache[require.resolve('../extension')];
  }
});

test('local Doxygen HTML wins; missing local page falls back to published reference', async (t) => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'xengine-hover-local-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const html = path.join(root, 'build', 'docs', 'game', 'html', 'classGameObject.html');
  const xml = path.join(root, 'build', 'docs', 'game', 'xml', 'index.xml');
  const gameSource = path.join(root, 'games', 'Sample', 'src', 'main.cpp');
  fs.mkdirSync(path.join(root, 'engine', 'include'), { recursive: true });
  fs.mkdirSync(path.join(root, 'tools'), { recursive: true });
  fs.mkdirSync(path.dirname(gameSource), { recursive: true });
  fs.mkdirSync(path.dirname(html), { recursive: true });
  fs.mkdirSync(path.dirname(xml), { recursive: true });
  fs.writeFileSync(path.join(root, 'tools', 'vscode.py'), '# marker\n');
  fs.writeFileSync(xml, XML);
  fs.writeFileSync(html, '<html id="a123"></html>');

  let provider;
  let requests = 0;
  const registered = new Map();
  const opened = [];
  const vscode = {
    languages: {
      registerHoverProvider: (_selector, candidate) => {
        provider = candidate;
        return { dispose() {} };
      },
    },
    workspace: {
      getConfiguration: () => ({ get: (_key, fallback) => fallback }),
      onDidChangeConfiguration: () => ({ dispose() {} }),
    },
    window: {
      createOutputChannel: () => ({ appendLine() {}, dispose() {} }),
      showInformationMessage() {},
      showWarningMessage() {},
    },
    commands: {
      registerCommand: (name, callback) => {
        registered.set(name, callback);
        return { dispose() {} };
      },
      executeCommand: async () => [],
    },
    env: { openExternal: async uri => { opened.push(uri); return true; } },
    Uri: { parse: value => value },
    MarkdownString: class {
      constructor(value) { this.value = value; }
    },
    Hover: class {
      constructor(contents) { this.contents = contents; }
    },
  };

  const originalLoad = Module._load;
  const originalFetch = global.fetch;
  try {
    Module._load = function (name, ...rest) {
      return name === 'vscode' ? vscode : originalLoad.call(this, name, ...rest);
    };
    global.fetch = async () => {
      requests++;
      return { ok: true, text: async () => XML };
    };
    const { activate } = require('../extension');
    activate({ subscriptions: [] });

    let hoveredSymbol = 'SetActive';
    const document = {
      uri: { fsPath: gameSource },
      getWordRangeAtPosition: () => ({ start: { character: 12 } }),
      getText: () => hoveredSymbol,
      lineAt: () => ({ text: `GameObject::${hoveredSymbol}()` }),
    };
    const position = { line: 0, character: 14 };
    const token = { isCancellationRequested: false };

    const localHover = await provider.provideHover(document, position, token);
    assert.match(localHover.contents.value, /command:xengineDoxygen\.openLocal\?/);
    assert.match(localHover.contents.value, /Local Doxygen reference/);
    assert.deepEqual(localHover.contents.isTrusted,
      { enabledCommands: ['xengineDoxygen.openLocal'] });
    assert.equal(requests, 0, 'A valid local reference must not access GitHub');

    const commandUrl = localHover.contents.value.match(/\]\((command:[^)]+)\)/)[1];
    const args = JSON.parse(decodeURIComponent(commandUrl.split('?')[1]));
    assert.equal(args[0].startsWith('file://'), true);
    assert.equal(args[0].endsWith('classGameObject.html#a123'), true);
    assert.equal(args[1],
      'https://the-33.github.io/XEngine/game/html/classGameObject.html#a123');
    await registered.get('xengineDoxygen.openLocal')(...args);
    assert.equal(opened[0], args[0]);

    fs.unlinkSync(html);
    const onlineHover = await provider.provideHover(document, position, token);
    assert.match(onlineHover.contents.value,
      /https:\/\/the-33\.github\.io\/XEngine\/game\/html\/classGameObject\.html#a123/);
    assert.equal(onlineHover.contents.isTrusted, undefined);
    assert.equal(requests, 1);

    // A regenerated local index should become visible without reloading VS Code.
    fs.writeFileSync(html, '<html id="a123"></html>');
    fs.writeFileSync(xml, XML.replace('<name>SetActive</name>', '<name>NewMethod</name>'));
    const modified = new Date(Date.now() + 5000);
    fs.utimesSync(xml, modified, modified);
    hoveredSymbol = 'NewMethod';
    const refreshed = await provider.provideHover(document, position, token);
    assert.match(refreshed.contents.value, /Local Doxygen reference/);
    assert.equal(requests, 1, 'Regenerating local docs must not require a network lookup');

    const marker = path.join(root, 'build', 'ide', 'doxygen_online_only');
    fs.mkdirSync(path.dirname(marker), { recursive: true });
    fs.writeFileSync(marker, 'online\n');
    hoveredSymbol = 'SetActive';
    const onlineOnly = await provider.provideHover(document, position, token);
    assert.match(onlineOnly.contents.value, /https:\/\/the-33\.github\.io/);
    assert.doesNotMatch(onlineOnly.contents.value, /Local Doxygen reference/);

    fs.unlinkSync(marker);
    hoveredSymbol = 'NewMethod';
    const restored = await provider.provideHover(document, position, token);
    assert.match(restored.contents.value, /Local Doxygen reference/);
  } finally {
    Module._load = originalLoad;
    global.fetch = originalFetch;
    delete require.cache[require.resolve('../extension')];
  }
});
