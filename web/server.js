const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');
const multer = require('multer');
const os = require('os');
const { exec } = require('child_process');

// Store uploads in temp directory
const upload = multer({ dest: os.tmpdir() });

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());


//CP1: Health check
app.get('/api/health', (req, res) => {
    res.json({ ok: true });
});

//CP2: List available results
app.get('/api/results', (req, res) => {
    const outDir = path.join(__dirname, '..', 'out');
    try {
        const files = fs.readdirSync(outDir)
            .filter(f => f.endsWith('.json'))
            .map(f => f.replace('.json', ''));
        res.json({ ok: true, results: files });
    } catch (err) {
        res.json({ ok: false, error: { code: 'DIR_ERROR', message: err.message } });
    }
});

//CP2: Serve analysis JSON
app.get('/api/results/:stem', (req, res) => {
    const filePath = path.join(__dirname, '..', 'out', `${req.params.stem}.json`);
    if (!fs.existsSync(filePath)) {
        return res.status(404).json({ ok: false, error: { code: 'NOT_FOUND', message: `No results for ${req.params.stem}` } });
    }
    try {
        const data = JSON.parse(fs.readFileSync(filePath, 'utf8'));
        res.json(data);
    } catch (err) {
        res.status(500).json({ ok: false, error: { code: 'READ_ERROR', message: err.message } });
    }
});

// Replace existing /api/analyze with this:
app.post('/api/analyze', upload.fields([
    { name: 'blk', maxCount: 1 },
    { name: 'rev', maxCount: 1 },
    { name: 'xor', maxCount: 1 }
]), (req, res) => {
    const files = req.files;
    if (!files?.blk || !files?.rev || !files?.xor) {
        return res.status(400).json({ ok: false, error: { code: 'INVALID_ARGS', message: 'blk, rev, xor files required' } });
    }

    const blkPath = files.blk[0].path;
    const revPath = files.rev[0].path;
    const xorPath = files.xor[0].path;

    const cmd = `./block_parser --ui ${blkPath} ${revPath} ${xorPath}`;
    exec(cmd, { cwd: path.join(__dirname, '..'), maxBuffer: 1024 * 1024 * 50 }, (error, stdout, stderr) => {
        // cleanup temp files
        fs.unlinkSync(blkPath);
        fs.unlinkSync(revPath);
        fs.unlinkSync(xorPath);

        if (error && !stdout) {
            return res.status(500).json({ ok: false, error: { code: 'EXEC_ERROR', message: error.message } });
        }
        try {
            const result = JSON.parse(stdout);
            res.json(result);
        } catch (err) {
            res.status(500).json({ ok: false, error: { code: 'PARSE_ERROR', message: 'Failed to parse CLI output' } });
        }
    });
});

//serving React build:
app.use(express.static(path.join(__dirname, 'client/dist')));

//all unknown routes serve index.html:
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'client/dist/index.html'));
});

app.listen(PORT, () => {});
