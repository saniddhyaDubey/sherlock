import { useState, useEffect } from "react";
import { AnalysisResult } from "@/types/analysis";

const API_BASE = "";

export function useAnalysisData() {
  const [availableFiles, setAvailableFiles] = useState<string[]>([]);
  const [selectedFile, setSelectedFile] = useState<string>("");
  const [data, setData] = useState<AnalysisResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  //fetching available files on mount:
  useEffect(() => {
    fetch(`${API_BASE}/api/results`)
      .then(res => res.json())
      .then(json => {
        if (json.ok && json.results.length > 0) {
          setAvailableFiles(json.results);
          setSelectedFile(json.results[0]);
        }
      })
      .catch(err => setError(err.message));
  }, []);

  //fetch analysis data when selected file changes:
  useEffect(() => {
    if (!selectedFile) return;
    setLoading(true);
    setError(null);
    fetch(`${API_BASE}/api/results/${selectedFile}`)
      .then(res => res.json())
      .then(json => {
        setData(json);
        setLoading(false);
      })
      .catch(err => {
        setError(err.message);
        setLoading(false);
      });
  }, [selectedFile]);

  const selectFile = (file: string) => {
    setSelectedFile(file);
  };

  return { data, selectedFile, selectFile, loading, error, availableFiles };
}
