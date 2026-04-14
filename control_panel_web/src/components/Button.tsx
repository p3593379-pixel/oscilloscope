// FILE: control_panel_web/src/components/Button.tsx
import React from 'react';
import styles from './Button.module.css';

interface Props extends React.ButtonHTMLAttributes<HTMLButtonElement> {
    variant?: 'primary' | 'secondary' | 'danger' | 'ghost';
    size?: 'sm' | 'md';
}

export default function Button({ variant = 'primary', size = 'md', className = '', ...props }: Props) {
    return (
        <button
            className={`${styles.btn} ${styles[variant]} ${styles[size]} ${className}`}
            {...props}
        />
    );
}
