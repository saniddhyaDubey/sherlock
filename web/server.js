const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');
const multer = require('multer');
const os = require('os');
const { spawn } = require('child_process');

const MAX_FILE_MB = 20;

// Store uploads in temp directory, limit file size to avoid OOM on free tier
const upload = multer({
    dest: os.tmpdir(),
    limits: { fileSize: MAX_FILE_MB * 1024 * 1024 }
});

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

app.post('/api/analyze', (req, res, next) => {
    upload.fields([
        { name: 'blk', maxCount: 1 },
        { name: 'rev', maxCount: 1 },
        { name: 'xor', maxCount: 1 }
    ])(req, res, (err) => {
        if (err) {
            if (err.code === 'LIMIT_FILE_SIZE') {
                return res.status(413).json({ ok: false, error: { code: 'FILE_TOO_LARGE', message: `Each file must be under ${MAX_FILE_MB}MB. Upload smaller block files for live analysis.` } });
            }
            return res.status(400).json({ ok: false, error: { code: 'UPLOAD_ERROR', message: err.message } });
        }
        next();
    });
}, (req, res) => {
    const files = req.files;
    if (!files?.blk || !files?.rev || !files?.xor) {
        return res.status(400).json({ ok: false, error: { code: 'INVALID_ARGS', message: 'blk, rev, xor files required' } });
    }

    const blkPath = files.blk[0].path;
    const revPath = files.rev[0].path;
    const xorPath = files.xor[0].path;

    const cleanup = () => {
        [blkPath, revPath, xorPath].forEach(f => { try { fs.unlinkSync(f); } catch (_) {} });
    };

    const proc = spawn('./block_parser', ['--ui', blkPath, revPath, xorPath], {
        cwd: path.join(__dirname, '..')
    });

    const chunks = [];
    proc.stdout.on('data', (d) => chunks.push(d));
    proc.stderr.on('data', () => {}); // suppress stderr noise

    proc.on('close', (code) => {
        cleanup();
        const stdout = Buffer.concat(chunks).toString('utf8');
        if (!stdout && code !== 0) {
            return res.status(500).json({ ok: false, error: { code: 'EXEC_ERROR', message: 'Analyzer exited with no output' } });
        }
        try {
            const result = JSON.parse(stdout);
            res.json(result);
        } catch (err) {
            res.status(500).json({ ok: false, error: { code: 'PARSE_ERROR', message: 'Failed to parse analyzer output' } });
        }
    });

    proc.on('error', (err) => {
        cleanup();
        res.status(500).json({ ok: false, error: { code: 'SPAWN_ERROR', message: err.message } });
    });
});

//serving React build:
app.use(express.static(path.join(__dirname, 'client/dist')));

//all unknown routes serve index.html:
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'client/dist/index.html'));
});

app.listen(PORT, () => {});
