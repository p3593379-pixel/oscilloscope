// FILE: control_panel_web/src/components/PanelHeader.tsx
import React from 'react';
import styles from './PanelHeader.module.css';

interface Props {
    title: string;
    description?: string;
    actions?: React.ReactNode;
}

export default function PanelHeader({ title, description, actions }: Props) {
    return (
        <div className={styles.header}>
            <div>
                <h1 className={styles.title}>{title}</h1>
                {description && <p className={styles.desc}>{description}</p>}
            </div>
            {actions && <div className={styles.actions}>{actions}</div>}
        </div>
    );
}
