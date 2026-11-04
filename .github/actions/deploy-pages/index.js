// deploy-pages reads its build version from GITHUB_SHA when it runs (src/internal/context.js); the runner sets that
// variable for every step, so it is replaced here, in the action's own process, before deploy-pages loads.
const path = require('path')
process.env.GITHUB_SHA = process.env.INPUT_BUILD_VERSION
require(path.resolve(process.env.DEPLOY_PAGES_DIR, 'dist/index.js'))
