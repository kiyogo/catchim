import { useSyncExternalStore } from "react";

const subscribe = (listener: () => void) => {
  window.addEventListener("popstate", listener);
  return () => window.removeEventListener("popstate", listener);
};
export const navigate = (url: string, replace = false) => {
  window.history[replace ? "replaceState" : "pushState"]({}, "", `#${url}`);
  window.dispatchEvent(new PopStateEvent("popstate"));
};
const router = {
  push: (url: string) => navigate(url), replace: (url: string) => navigate(url, true),
  back: () => window.history.back(), refresh: () => window.location.reload(), prefetch: () => {},
};
export const useRouter = () => router;
export const usePathname = () => useSyncExternalStore(subscribe, () => location.hash.slice(1) || "/projects");
export const useParams = <T,>() => ({ project_id: usePathname().split("/")[2] } as T);
export const useSearchParams = () => new URLSearchParams(location.search);
