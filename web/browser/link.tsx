import type { AnchorHTMLAttributes } from "react";
import { navigate } from "./navigation";
export default function Link({ href, onClick, ...props }: AnchorHTMLAttributes<HTMLAnchorElement>) {
  return <a {...props} href={href?.startsWith("/") ? `#${href}` : href} onClick={event => {
    onClick?.(event);
    if (!event.defaultPrevented && href?.startsWith("/") && !event.ctrlKey && !event.metaKey && event.button === 0) {
      event.preventDefault(); navigate(href);
    }
  }} />;
}
