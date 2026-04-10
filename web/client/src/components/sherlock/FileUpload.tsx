import { useRef, useState } from "react";
import { Loader2, Upload } from "lucide-react";
import { AnalysisResult } from "@/types/analysis";

interface FileUploadProps {
  onAnalysisComplete: (data: AnalysisResult) => void;
}

export function FileUpload({ onAnalysisComplete }: FileUploadProps) {
  const blkRef = useRef<HTMLInputElement>(null);
  const revRef = useRef<HTMLInputElement>(null);
  const xorRef = useRef<HTMLInputElement>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadError, setUploadError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  const handleAnalyze = async () => {
    const blk = blkRef.current?.files?.[0];
    const rev = revRef.current?.files?.[0];
    const xor = xorRef.current?.files?.[0];
    if (!blk || !rev || !xor) {
      setUploadError("Please select all three files: blk.dat, rev.dat, xor.dat");
      return;
    }
    setUploading(true);
    setUploadError(null);
    setSuccess(null);
    const formData = new FormData();
    formData.append("blk", blk);
    formData.append("rev", rev);
    formData.append("xor", xor);
    try {
      const res = await fetch("/api/analyze", { method: "POST", body: formData });
      const json = await res.json();
      if (json.ok) {
        setSuccess(`ANALYSIS_COMPLETE — ${json.file}`);
        onAnalysisComplete(json);
      } else {
        setUploadError(json.error?.message || "Analysis failed");
      }
    } catch (err) {
      setUploadError("Failed to connect to server");
    } finally {
      setUploading(false);
    }
  };

  return (
    <div className="surface-card p-4 mb-4">
      <p className="text-label mb-4">ANALYZE_NEW_BLOCK</p>
      {/* Stack on mobile, 3-col on sm+ */}
      <div className="grid grid-cols-1 sm:grid-cols-3 gap-3 mb-4">
        {[
          { label: "BLK_FILE", ref: blkRef },
          { label: "REV_FILE", ref: revRef },
          { label: "XOR_FILE", ref: xorRef },
        ].map(({ label, ref }) => (
          <div key={label}>
            <p className="text-label mb-2">{label}</p>
            <input
              ref={ref}
              type="file"
              accept=".dat"
              className="w-full text-sm font-mono-data text-muted-foreground bg-secondary border border-border px-3 py-2 cursor-pointer file:mr-3 file:py-1 file:px-3 file:border file:border-border file:bg-background file:text-foreground file:text-xs file:font-mono-data file:cursor-pointer"
            />
          </div>
        ))}
      </div>
      <div className="flex flex-wrap items-center gap-3">
        <button
          onClick={handleAnalyze}
          disabled={uploading}
          className="flex items-center gap-2 border border-foreground px-4 py-2 text-sm font-medium hover:bg-foreground hover:text-background transition-colors disabled:opacity-50"
        >
          {uploading ? <Loader2 className="w-4 h-4 animate-spin" /> : <Upload className="w-4 h-4" />}
          {uploading ? "ANALYZING..." : "RUN_ANALYSIS"}
        </button>
        {uploadError && <p className="text-sm font-mono-data text-destructive">{uploadError}</p>}
        {success && <p className="text-sm font-mono-data text-[hsl(var(--simple-payment))]">{success}</p>}
      </div>
    </div>
  );
}
