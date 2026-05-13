import styles from './TriggerStubWidget.module.css';

export function TriggerStubWidget() {
    return (
        <div className={styles.stub}>
            <p className={styles.label}>Trigger</p>
            <div className={styles.row}>
                <label>Source</label>
                <select disabled>
                    <option>CH1</option>
                    <option>CH2</option>
                </select>
            </div>
            <div className={styles.row}>
                <label>Mode</label>
                <select disabled>
                    <option>Auto</option>
                    <option>Normal</option>
                    <option>Single</option>
                </select>
            </div>
            <div className={styles.row}>
                <label>Level</label>
                <input type="range" min={-5} max={5} defaultValue={0} disabled />
            </div>
            <p className={styles.note}>stub – not wired up yet</p>
        </div>
    );
}
