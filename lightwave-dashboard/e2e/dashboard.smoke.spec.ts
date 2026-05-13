import { test, expect } from './fixtures/test-fixtures';

test('dashboard boots and exposes K1 AP connection settings', async ({ page, dashboard }) => {
  await dashboard.goto();

  await expect(page.getByText('LightwaveOS')).toBeVisible();
  await dashboard.switchTab('System');
  await expect(page.getByLabel('Device Origin')).toHaveValue('http://192.168.4.1');
  await expect(page.getByRole('combobox')).toHaveValue('none');
});

test('primary tabs are reachable without K1 hardware', async ({ page, dashboard }) => {
  await dashboard.goto();

  for (const tab of ['Control', 'Shows', 'Effects', 'System'] as const) {
    await dashboard.switchTab(tab);
    await expect(page.getByRole('tab', { name: tab })).toHaveAttribute('aria-selected', 'true');
  }
});
