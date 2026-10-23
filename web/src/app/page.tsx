import type { Metadata } from "next";
import { redirect } from "next/navigation";
import { SITE_URL } from "@/site/brand";

export const metadata: Metadata = {
	alternates: {
		canonical: SITE_URL,
	},
};

export default function Home() {
	redirect("/projects");
}
