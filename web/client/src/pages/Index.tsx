import { useState } from "react";
import { Navbar } from "@/components/sherlock/NavBar";
import { FileUpload } from "@/components/sherlock/FileUpload";
import { OverviewSection } from "@/components/sherlock/OverviewSection";
import { BlockExplorer } from "@/components/sherlock/BlockExplorer";
import { HeuristicsLab } from "@/components/sherlock/HeuristicsLab";
import { useAnalysisData } from "@/hooks/use-analysis-data";
import { Loader2 } from "lucide-react";
import { AnalysisResult } from "@/types/analysis";
import { HowToUse } from "@/components/sherlock/HowToUse";

const Index = () => {
  const [activeTab, setActiveTab] = useState("Overview");
  const [uploadedData, setUploadedData] = useState<AnalysisResult | null>(null);
  const { data: committedData, selectedFile, selectFile, loading, availableFiles } = useAnalysisData();

  const data = uploadedData || committedData;

  return (
    <div className="min-h-screen bg-background text-foreground antialiased">
      <Navbar
        activeTab={activeTab}
        onTabChange={setActiveTab}
        selectedFile={selectedFile}
        onFileSelect={selectFile}
        availableFiles={availableFiles}
      />

      <main className="p-6 max-w-[1600px] mx-auto">
        <HowToUse />
        <FileUpload onAnalysisComplete={setUploadedData} />

        {loading ? (
          <div className="flex items-center justify-center py-32">
            <Loader2 className="w-6 h-6 animate-spin text-muted-foreground" />
            <span className="ml-3 text-sm font-mono-data text-muted-foreground">
              LOADING_ANALYSIS...
            </span>
          </div>
        ) : !data ? (
          <div className="flex items-center justify-center py-32">
            <p className="text-sm font-mono-data text-muted-foreground">
              ERR: NO_DATA_AVAILABLE. Select a committed file or upload new block files above.
            </p>
          </div>
        ) : (
          <>
            {activeTab === "Overview" && <OverviewSection data={data} />}
            {activeTab === "Explorer" && <BlockExplorer blocks={data.blocks} />}
            {activeTab === "Heuristics" && <HeuristicsLab data={data} />}
          </>
        )}
      </main>
    </div>
  );
};

export default Index;
