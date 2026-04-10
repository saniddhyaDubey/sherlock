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
    <nav className="border-b border-border">
      {/* Top row: brand + file selector */}
      <div className="flex items-center justify-between px-4 h-12">
        <h1 className="text-lg font-bold tracking-tighter text-foreground">SHERLOCK</h1>
        <FileSelector selected={selectedFile} onSelect={onFileSelect} availableFiles={availableFiles} />
      </div>
      {/* Bottom row: tabs, scrollable on mobile */}
      <div className="flex overflow-x-auto border-t border-border scrollbar-none">
        {tabs.map((tab) => (
          <button
            key={tab}
            onClick={() => onTabChange(tab)}
            className={`flex-1 min-w-[90px] px-3 py-2.5 text-sm font-medium whitespace-nowrap transition-colors ${
              activeTab === tab
                ? "text-foreground border-b-2 border-foreground"
                : "text-muted-foreground hover:text-foreground"
            }`}
          >
            {tab}
          </button>
        ))}
      </div>
    </nav>
  );
}
