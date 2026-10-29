'use strict';

// Doxygen's generated XML index is small enough to parse once per profile.
// Keeping these functions independent of VS Code makes them testable with
// the actual XML emitted by the XEngine documentation build.
const DEFAULT_SITE = 'https://the-33.github.io/XEngine/';
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const MEMBER_KINDS = new Set([
  'function', 'variable', 'typedef', 'enum', 'enumvalue',
  'define', 'property', 'signal', 'slot',
]);
const COMPOUND_KINDS = new Set([
  'class', 'struct', 'union', 'namespace', 'concept', 'group',
]);

function decodeXml(value) {
  return value.replace(/&(#x[0-9a-f]+|#[0-9]+|amp|lt|gt|quot|apos);/gi, (_, entity) => {
    const named = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'" };
    if (Object.hasOwn(named, entity.toLowerCase())) return named[entity.toLowerCase()];
    const point = entity[1].toLowerCase() === 'x'
      ? parseInt(entity.slice(2), 16) : parseInt(entity.slice(1), 10);
    return Number.isInteger(point) && point >= 0 && point <= 0x10ffff
      ? String.fromCodePoint(point) : '';
  });
}

function attr(tag, name) {
  const match = new RegExp(`\\b${name}="([^"]*)"`).exec(tag);
  return match ? decodeXml(match[1]) : '';
}

function normalizeBaseUrl(raw) {
  const url = new URL(raw || DEFAULT_SITE);
  if (url.protocol !== 'https:' || url.username || url.password || url.search || url.hash) {
    throw new Error('Documentation base URL must be a plain HTTPS URL.');
  }
  url.pathname = url.pathname.replace(/\/?$/, '/');
  return url.toString();
}

function parseIndexAtRoot(xml, profile, root) {
  if (!['game', 'engine'].includes(profile)) throw new Error('Unknown Doxygen profile');
  if (!/<doxygenindex\b/.test(xml)) throw new Error('Not a Doxygen index.xml file');
  const symbols = new Map();
  function add(name, record) {
    if (!name || !/^[A-Za-z0-9_]+$/.test(record.refid)) return;
    const records = symbols.get(name) || [];
    if (!records.some(item => item.url === record.url)) records.push(record);
    symbols.set(name, records);
  }

  const compounds = /<compound\b([^>]*)>([\s\S]*?)<\/compound>/g;
  let match;
  while ((match = compounds.exec(xml)) !== null) {
    const refid = attr(match[1], 'refid');
    const kind = attr(match[1], 'kind');
    const heading = /<name>([\s\S]*?)<\/name>/.exec(match[2]);
    if (!refid || !heading || !/^[A-Za-z0-9_]+$/.test(refid)) continue;
    const compoundName = decodeXml(heading[1]);
    const classUrl = `${root}${refid}.html`;
    if (COMPOUND_KINDS.has(kind)) {
      add(compoundName.split('::').at(-1), {
        name: compoundName, owner: compoundName, refid,
        kind: 'compound', profile, url: classUrl,
      });
    }

    const members = /<member\b([^>]*)>\s*<name>([\s\S]*?)<\/name>\s*<\/member>/g;
    let member;
    while ((member = members.exec(match[2])) !== null) {
      const memberRef = attr(member[1], 'refid');
      const memberKind = attr(member[1], 'kind');
      if (!MEMBER_KINDS.has(memberKind) ||
          !memberRef.startsWith(`${refid}_1`)) continue;
      const anchor = memberRef.slice(refid.length + 2);
      if (!/^[A-Za-z0-9_]+$/.test(anchor)) continue;
      const memberName = decodeXml(member[2]);
      if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(memberName)) continue;
      add(memberName, {
        name: memberName, owner: compoundName, refid,
        kind: memberKind, profile, url: `${classUrl}#${anchor}`,
      });
    }
  }

  if (symbols.size === 0) throw new Error('Doxygen index has no recognized symbols');
  return symbols;
}

function parseIndex(xml, profile, baseUrl = DEFAULT_SITE) {
  const root = new URL(`${profile}/html/`, normalizeBaseUrl(baseUrl)).toString();
  return parseIndexAtRoot(xml, profile, root);
}

function parseLocalIndex(xml, profile, htmlDirectory) {
  // pathToFileURL encodes spaces, non-ASCII and Windows drive letters correctly.
  const root = pathToFileURL(path.resolve(htmlDirectory) + path.sep).toString();
  return parseIndexAtRoot(xml, profile, root);
}

function localCommandLink(fileUrl, fallbackUrl) {
  const parsed = new URL(fileUrl);
  const fallback = new URL(fallbackUrl);
  if (parsed.protocol !== 'file:' || !parsed.pathname.endsWith('.html')) {
    throw new Error('Not a local Doxygen HTML page');
  }
  if (fallback.protocol !== 'https:') throw new Error('Fallback must use HTTPS');
  return `command:xengineDoxygen.openLocal?${encodeURIComponent(JSON.stringify([fileUrl, fallbackUrl]))}`;
}

function chooseProfile(fileName, configured = 'auto') {
  if (configured === 'game' || configured === 'engine') return configured;
  return /(?:^|[\\/])engine[\\/](?:include|src)[\\/]/i.test(fileName)
    ? 'engine' : 'game';
}

function scopeBefore(line, offset) {
  const prefix = line.slice(0, offset);
  const scoped = /([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*::\s*$/.exec(prefix);
  return scoped ? scoped[1] : '';
}

function findLinks(index, symbol, options = {}) {
  let records = index.get(symbol) || [];
  if (!records.length) return [];

  // For a class token, its dedicated page is always the best target.
  const compound = records.filter(record => record.kind === 'compound');
  if (compound.length) return compound.slice(0, 4);

  if (options.scope) {
    records = records.filter(record => record.owner === options.scope ||
      record.owner.endsWith(`::${options.scope}`));
  } else if (options.definitions?.length) {
    // Prefer the owner named by Go to Definition's header file. Generic member
    // names must not link to arbitrary classes in unrelated games.
    const paths = options.definitions.map(value => value.replace(/\\/g, '/').toLowerCase());
    const engineDefinitions = paths.filter(value =>
      /\/(?:engine\/(?:include|src)|xengine\/include)\//.test(value));
    if (!engineDefinitions.length) return [];
    const basenames = engineDefinitions.map(value =>
      value.split('/').at(-1).replace(/\.(?:h|hpp|hh|cpp|cc|cxx)$/i, ''));
    const selected = records.filter(record => basenames.some(base => {
      const owner = record.owner.toLowerCase();
      return owner === base || owner.startsWith(`${base}::`) ||
        owner.endsWith(`::${base}`);
    }));
    if (selected.length) records = selected;
    else if (new Set(records.map(record => record.owner)).size !== 1) return [];
  } else if (new Set(records.map(record => record.owner)).size !== 1) {
    // Without class context or definition data, we cannot identify the owner.
    return [];
  }

  return records.slice(0, 4);
}

function formatHover(records) {
  if (!records.length) return '';
  if (records.length === 1) {
    return `[📖 Open in XEngine documentation](${records[0].url})`;
  }
  return '**📖 XEngine documentation**\n\n' + records.map((record, index) => {
    const label = record.kind === 'compound'
      ? record.owner : `${record.owner}::${record.name}${records.filter(r =>
          r.name === record.name && r.owner === record.owner).length > 1
          ? ` (overload ${index + 1})` : ''}`;
    return `- [${label}](${record.url})`;
  }).join('\n');
}

module.exports = {
  DEFAULT_SITE, normalizeBaseUrl, parseIndex, parseLocalIndex, localCommandLink,
  chooseProfile, scopeBefore, findLinks, formatHover,
};
