'use strict';

const assert = require('node:assert/strict');
const Module = require('node:module');
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
        get: (_name, fallback) => fallback,
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
