// FILE: control_panel_web/src/components/FormField.tsx
import React from 'react';
import styles from './FormField.module.css';

interface Props {
    label: string;
    hint?: string;
    children: React.ReactNode;
}

export default function FormField({ label, hint, children }: Props) {
    return (
        <div className={styles.field}>
            <label className={styles.label}>{label}</label>
            {children}
            {hint && <p className={styles.hint}>{hint}</p>}
        </div>
    );
}
