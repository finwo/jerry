#!/usr/bin/env node

const esbuild = require('esbuild');
const { NodeGlobalsPolyfillPlugin } = require('@esbuild-plugins/node-globals-polyfill');

esbuild.build({
  entryPoints : [__dirname + '/app.ts'],
  bundle      : true,
  outfile     : __dirname + '/app.js',
  define      : {
    global : 'window',
  },
  plugins     : [
    NodeGlobalsPolyfillPlugin({
      buffer : true
    })
  ]
});


