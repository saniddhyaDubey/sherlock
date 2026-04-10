import { FileSelector } from "./FileSelector";

interface NavbarProps {
  activeTab: string;
  onTabChange: (tab: string) => void;
  selectedFile: string;
  onFileSelect: (file: string) => void;
  availableFiles: string[];
}

const tabs = ["Overview", "Explorer", "Heuristics"];

export function Navbar({ activeTab, onTabChange, selectedFile, onFileSelect, availableFiles }: NavbarProps) {
  return (
    <nav className="border-b border-border h-16 flex items-center justify-between px-6">
      <div className="flex items-center gap-8">
        <h1 className="text-xl font-bold tracking-tighter text-foreground">SHERLOCK</h1>
        <div className="flex gap-1">
          {tabs.map((tab) => (
            <button
              key={tab}
              onClick={() => onTabChange(tab)}
              className={`px-4 py-2 text-sm font-medium transition-colors ${
                activeTab === tab
                  ? "text-foreground border-b-2 border-foreground"
                  : "text-muted-foreground hover:text-foreground"
              }`}
            >
              {tab}
            </button>
          ))}
        </div>
      </div>
      <FileSelector selected={selectedFile} onSelect={onFileSelect} availableFiles={availableFiles} />
    </nav>
  );
}
