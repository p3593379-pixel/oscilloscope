import styles from './MeasurementsStubWidget.module.css';

const ROWS = [
    { label: 'Freq',   value: '—',     unit: 'Hz'  },
    { label: 'Period', value: '—',     unit: 'ms'  },
    { label: 'Vpp',    value: '—',     unit: 'V'   },
    { label: 'Vmax',   value: '—',     unit: 'V'   },
    { label: 'Vmin',   value: '—',     unit: 'V'   },
    { label: 'RMS',    value: '—',     unit: 'V'   },
];

export function MeasurementsStubWidget() {
    return (
        <div className={styles.stub}>
            <p className={styles.label}>Measurements</p>
            <table className={styles.table}>
                <tbody>
                {ROWS.map((r) => (
                    <tr key={r.label}>
                        <td className={styles.name}>{r.label}</td>
                        <td className={styles.value}>{r.value}</td>
                        <td className={styles.unit}>{r.unit}</td>
                    </tr>
                ))}
                </tbody>
            </table>
            <p className={styles.note}>stub – not wired up yet</p>
        </div>
    );
}
