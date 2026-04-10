import { Classification } from "@/types/analysis";

interface ClassificationBadgeProps {
  classification: Classification;
}

const badgeClasses: Record<Classification, string> = {
  coinjoin: "badge-coinjoin",
  consolidation: "badge-consolidation",
  self_transfer: "badge-self-transfer",
  simple_payment: "badge-simple-payment",
  batch_payment: "badge-batch-payment",
  unknown: "badge-unknown",
};

export function ClassificationBadge({ classification }: ClassificationBadgeProps) {
  return (
    <span
      className={`inline-flex px-2 py-0.5 text-[10px] font-bold uppercase tracking-wider ${badgeClasses[classification]}`}
    >
      {classification.replace("_", " ")}
    </span>
  );
}
