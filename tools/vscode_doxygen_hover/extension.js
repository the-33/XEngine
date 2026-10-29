'use strict';

const vscode = require('vscode');
const {
  DEFAULT_SITE, normalizeBaseUrl, parseIndex, chooseProfile,
  scopeBefore, findLinks, formatHover,
} = require('./index');

const INDEX_TTL = 30 * 60 * 1000;
const REQUEST_TIMEOUT = 6000;

function activate(context) {
  const cache = new Map();
  const output = vscode.window.createOutputChannel('XEngine Doxygen Hover');
  context.subscriptions.push(output);

  function settings() {
    const config = vscode.workspace.getConfiguration('xengineDoxygen');
    const baseUrl = normalizeBaseUrl(config.get('baseUrl', DEFAULT_SITE));
    return {
      enabled: config.get('enabled', true),
      profile: config.get('profile', 'auto'),
      baseUrl,
    };
  }

  async function fetchIndex(profile, baseUrl) {
    const key = `${baseUrl}|${profile}`;
    const entry = cache.get(key);
    if (entry && entry.expires > Date.now()) return entry.promise;

    const promise = (async () => {
      const url = new URL(`${profile}/xml/index.xml`, baseUrl).toString();
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), REQUEST_TIMEOUT);
      try {
        const response = await fetch(url, { signal: controller.signal });
        if (!response.ok) throw new Error(`HTTP ${response.status} from ${url}`);
        const xml = await response.text();
        return parseIndex(xml, profile, baseUrl);
      } finally {
        clearTimeout(timeout);
      }
    })();

    cache.set(key, { promise, expires: Date.now() + INDEX_TTL });
    promise.catch(error => {
      cache.delete(key);
      output.appendLine(`[WARN] Unable to load ${profile} Doxygen index: ${error.message}`);
    });
    return promise;
  }

  async function definitionsAt(document, position) {
    try {
      const definitions = await vscode.commands.executeCommand(
        'vscode.executeDefinitionProvider', document.uri, position);
      return (Array.isArray(definitions) ? definitions : [])
        .map(result => (result.targetUri || result.uri)?.fsPath)
        .filter(Boolean);
    } catch (error) {
      output.appendLine(`[WARN] C++ definition provider unavailable: ${error.message}`);
      return [];
    }
  }

  context.subscriptions.push(vscode.languages.registerHoverProvider(
    { language: 'cpp', scheme: 'file' },
    {
      async provideHover(document, position, token) {
        let config;
        try { config = settings(); }
        catch (error) {
          output.appendLine(`[ERROR] Invalid documentation URL: ${error.message}`);
          return undefined;
        }
        if (!config.enabled || token.isCancellationRequested) return undefined;

        const range = document.getWordRangeAtPosition(position);
        if (!range) return undefined;
        const symbol = document.getText(range);
        if (!/^[A-Za-z_][A-Za-z_0-9]*$/.test(symbol)) return undefined;

        const profile = chooseProfile(document.uri.fsPath, config.profile);
        let index;
        try { index = await fetchIndex(profile, config.baseUrl); }
        catch (_) { return undefined; }
        if (token.isCancellationRequested || !index.has(symbol)) return undefined;

        const line = document.lineAt(position.line).text;
        const scope = scopeBefore(line, range.start.character);
        const possible = index.get(symbol) || [];
        const isClass = possible.some(record => record.kind === 'compound');
        const definitions = (scope || isClass) ? [] : await definitionsAt(document, position);
        if (token.isCancellationRequested) return undefined;

        const links = findLinks(index, symbol, { scope, definitions });
        if (!links.length) return undefined;
        const markdown = new vscode.MarkdownString(formatHover(links));
        // HTTPS links work without trusted command links or raw HTML.
        return new vscode.Hover(markdown, range);
      },
    },
  ));

  context.subscriptions.push(vscode.commands.registerCommand(
    'xengineDoxygen.openPortal', async () => {
      try {
        await vscode.env.openExternal(vscode.Uri.parse(settings().baseUrl));
      } catch (error) {
        vscode.window.showErrorMessage(`Cannot open XEngine documentation: ${error.message}`);
      }
    },
  ));

  context.subscriptions.push(vscode.commands.registerCommand(
    'xengineDoxygen.refreshIndex', async () => {
      cache.clear();
      try {
        const config = settings();
        const [game, engine] = await Promise.all([
          fetchIndex('game', config.baseUrl),
          fetchIndex('engine', config.baseUrl),
        ]);
        vscode.window.showInformationMessage(
          `XEngine Doxygen indexes refreshed (${game.size} game / ${engine.size} engine symbol names).`);
      } catch (error) {
        vscode.window.showWarningMessage(
          `XEngine Doxygen index unavailable: ${error.message}. See Output → XEngine Doxygen Hover.`);
      }
    },
  ));

  context.subscriptions.push(vscode.workspace.onDidChangeConfiguration(event => {
    if (event.affectsConfiguration('xengineDoxygen')) cache.clear();
  }));
}

function deactivate() {}

module.exports = { activate, deactivate };
