'use strict';

const vscode = require('vscode');
const fs = require('node:fs');
const path = require('node:path');
const { fileURLToPath } = require('node:url');
const {
  DEFAULT_SITE, normalizeBaseUrl, parseIndex, parseLocalIndex,
  localCommandLink, chooseProfile, scopeBefore, findLinks, formatHover,
} = require('./index');

const INDEX_TTL = 30 * 60 * 1000;
const REQUEST_TIMEOUT = 6000;

function findXEngineRoot(filePath) {
  let current = path.dirname(path.resolve(filePath));
  for (let depth = 0; depth < 15; depth++) {
    if (fs.existsSync(path.join(current, 'engine', 'include')) &&
        fs.existsSync(path.join(current, 'tools', 'vscode.py'))) return current;
    const parent = path.dirname(current);
    if (parent === current) break;
    current = parent;
  }
  return null;
}

function activate(context) {
  const cache = new Map();
  const localCache = new Map();
  const output = vscode.window.createOutputChannel('XEngine Doxygen Hover');
  context.subscriptions.push(output);

  function settings() {
    const config = vscode.workspace.getConfiguration('xengineDoxygen');
    const baseUrl = normalizeBaseUrl(config.get('baseUrl', DEFAULT_SITE));
    return {
      enabled: config.get('enabled', true),
      preferLocal: config.get('preferLocal', true),
      profile: config.get('profile', 'auto'),
      baseUrl,
    };
  }

  async function localIndex(profile, sourcePath) {
    const root = findXEngineRoot(sourcePath);
    if (!root) return null;
    // A successful build with --no-doc opts this checkout into online-only
    // references without touching any personal VS Code settings.
    if (fs.existsSync(path.join(root, 'build', 'ide', 'doxygen_online_only'))) {
      return null;
    }
    const docs = path.join(root, 'build', 'docs', profile);
    const indexPath = path.join(docs, 'xml', 'index.xml');
    try {
      const stat = await fs.promises.stat(indexPath);
      if (!stat.isFile()) return null;
      const cached = localCache.get(indexPath);
      if (cached && cached.mtimeMs === stat.mtimeMs && cached.size === stat.size) {
        return cached.index;
      }
      const xml = await fs.promises.readFile(indexPath, 'utf8');
      const index = parseLocalIndex(xml, profile, path.join(docs, 'html'));
      localCache.set(indexPath, { mtimeMs: stat.mtimeMs, size: stat.size, index });
      return index;
    } catch (error) {
      if (error.code !== 'ENOENT') {
        output.appendLine(`[WARN] Local ${profile} Doxygen index unavailable: ${error.message}`);
      }
      localCache.delete(indexPath);
      return null;
    }
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
        const line = document.lineAt(position.line).text;
        const scope = scopeBefore(line, range.start.character);
        let definitionPromise;
        async function resolveLinks(index) {
          const possible = index?.get(symbol) || [];
          if (!possible.length || token.isCancellationRequested) return [];
          const isClass = possible.some(record => record.kind === 'compound');
          if (!scope && !isClass && !definitionPromise) {
            definitionPromise = definitionsAt(document, position);
          }
          const definitions = scope || isClass ? [] : await definitionPromise;
          return findLinks(index, symbol, { scope, definitions });
        }

        if (config.preferLocal) {
          const index = await localIndex(profile, document.uri.fsPath);
          const candidates = await resolveLinks(index);
          const available = candidates.filter(record =>
            fs.existsSync(fileURLToPath(record.url)));
          if (available.length && !token.isCancellationRequested) {
            const links = available.map(record => {
              const anchor = new URL(record.url).hash;
              const fallback = new URL(
                `${profile}/html/${record.refid}.html${anchor}`, config.baseUrl).toString();
              return { ...record, url: localCommandLink(record.url, fallback) };
            });
            const markdown = new vscode.MarkdownString(
              formatHover(links) + '\n\n_Local Doxygen reference_');
            // Trust only our own command, never arbitrary commands in Doxygen XML.
            markdown.isTrusted = { enabledCommands: ['xengineDoxygen.openLocal'] };
            return new vscode.Hover(markdown, range);
          }
        }

        let index;
        try { index = await fetchIndex(profile, config.baseUrl); }
        catch (_) { return undefined; }
        const links = await resolveLinks(index);
        if (!links.length || token.isCancellationRequested) return undefined;
        return new vscode.Hover(new vscode.MarkdownString(formatHover(links)), range);
      },
    },
  ));

  context.subscriptions.push(vscode.commands.registerCommand(
    'xengineDoxygen.openLocal', async (localUrl, remoteUrl) => {
      try {
        const local = new URL(localUrl);
        const remote = new URL(remoteUrl);
        if (local.protocol !== 'file:' || !local.pathname.endsWith('.html') ||
            remote.protocol !== 'https:') throw new Error('Invalid documentation URL');
        const exists = fs.existsSync(fileURLToPath(local));
        if (!exists || !(await vscode.env.openExternal(vscode.Uri.parse(localUrl)))) {
          await vscode.env.openExternal(vscode.Uri.parse(remoteUrl));
        }
      } catch (error) {
        vscode.window.showWarningMessage(
          `Cannot open local XEngine documentation: ${error.message}`);
      }
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
      localCache.clear();
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
    if (event.affectsConfiguration('xengineDoxygen')) {
      cache.clear();
      localCache.clear();
    }
  }));
}

function deactivate() {}

module.exports = { activate, deactivate };
