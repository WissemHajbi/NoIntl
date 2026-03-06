"use client";
import { useTranslations } from "next-intl";

// This is a translated component
export function SignInForm() {
  const t = useTranslations("modules.auth.signIn");
  const errors = useTranslations("modules.auth.errors");

  // SAFE examples — should NOT be flagged
  const title = t("title");
  const sub = t("subtitle");
  toast.success(t("actions.success"));
  toast.error(errors("general.serverError"));
  <Input placeholder={t("fields.email.placeholder")} />
  z.string().min(1, { message: t("name.required") });
  setError(errors("password.incorrect"));

  // FLAGGED examples — should all be caught
  return (
    <div>
      <h2>Welcome back</h2>
      <Input placeholder="Enter your email" />
      <Button title="Submit the form">Save Changes</Button>
      <p aria-label="Main content section">Hello World</p>
      {"Click here to continue"}
    </div>
  );
}

// Toast with hardcoded string — FLAGGED
toast.error("Something went wrong");
toast.success("Saved successfully");

// setError with literal — FLAGGED
setError("Invalid email address");
setWarning("Please check your input");

// Zod schema — FLAGGED
z.string().min(3, "Full name is required.");
z.object({ message: "Email is invalid." });
const schema = z.string().email("Must be a valid email");

// throw — FLAGGED
throw new Error("Unexpected state reached");
