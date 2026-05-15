
---

Temper 1.0
	[50%] Basic validation (new transaction) 
	[] Memo location and implementation. currently displayed in txn_det, but input in txn_main.
	[] lock restricted funds when unrestricted account selected.  Autopopulate General Funds?  flag default?
	[] Double click entry to load edit window
	[] Delete capability from ledger/Edit Window (non C/R)
	[] Delete txn_details in edit/new window
	[] Open Attachment button in txn edit window/ledger?
	[] 
	
Temper 1.0.1: 
	[] Fix Dialog Box Ghosting
	[] onNewTransaction - Table enhancements.
	[] Memo entry pop out for full display of memo only
	[] column sizing
	[] no horizontal scrolling required
	[] Update New Transaction dialog to support "Add Restricted Contribution" Templates

	- Dashboard Improvements
	- Reports Stub
	- add key fund balances to dashboard.  maybe put a "key" flag in the tables.

Temper 1.0.3:
	[] Reconciliation Framework
	
Temper 1.1: 
	[] Add Budgeting

Temper 1.2: 
	[] Add a Reconcile Bank Statement Tool



**NOTES:
Fix Qt Dialog Ghost: 
- That visual artifact (warped duplicate ~1 cm above the real dialog) is almost certainly caused by your KDE Plasma window manager (KWin) and its "card shuffle" animation when a window loses focus.

- Qt dialogs + KDE's compositor often fight over stacking order and rendering during the animation, leaving a "ghost" frame behind. It's a common Qt6 + KDE issue and doesn't affect functionality — just looks odd.

- We can deal with it later (a simple setWindowFlags or compositor tweak usually fixes it).