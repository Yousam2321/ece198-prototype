"""ECE198 Results: Part A rep counts and Part B hold/chest measurements."""
import csv
import os
from pathlib import Path
import queue
import sys
import tempfile
import threading
import time
import webbrowser
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Source checkout and PyInstaller both use these imports.
ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path: sys.path.insert(0, str(ROOT / "tools"))
from export_sessions import read_history, write_csv
from evaluation import (EXERCISES, ResultStore, criteria_text, evaluate, load_csv,
                        normalize, progression, validate_criteria, attempt_number, COUNT_EXERCISES)
from presentation import result_label, short_feedback, progress_summary
from report_data import preview_values, export_assessment_csv
from importing import read_all_history
from clean_report import build_report


def data_directory():
    base = Path(os.environ.get("LOCALAPPDATA", Path.home() / ".local" / "share"))
    return base / "ECE198_Results"


class ResultsApp:
    def __init__(self, root, store_path=None):
        self.root = root
        self.store = ResultStore(store_path or data_directory() / "results.sqlite3")
        self.rows = []
        self.confirmed = False
        self.source_label = "No results loaded"
        self.jobs = queue.Queue()
        self.busy = False
        root.title("ECE198 — Exercise Results")
        width=min(1080,root.winfo_screenwidth()-60)
        height=min(900,root.winfo_screenheight()-105)
        root.geometry(f"{width}x{height}+20+20")
        root.minsize(860, 560)
        root.configure(bg="#f3f6fb")
        style=ttk.Style(root)
        style.theme_use("clam")
        style.configure(".", font=("Segoe UI", 10))
        style.configure("TFrame", background="#f3f6fb")
        style.configure("TLabel", background="#f3f6fb", foreground="#17283d")
        style.configure("Title.TLabel", font=("Segoe UI", 21, "bold"))
        style.configure("Heading.TLabel", font=("Segoe UI", 12, "bold"))
        style.configure("TButton", padding=(10, 7))
        style.configure("Treeview", rowheight=26, font=("Segoe UI", 10))
        style.configure("Treeview.Heading", font=("Segoe UI", 10, "bold"))
        style.configure("TLabelframe",background="#f3f6fb")
        style.configure("TLabelframe.Label",background="#f3f6fb",foreground="#17283d")
        viewport=tk.Canvas(root,bg="#f3f6fb",highlightthickness=0)
        main_scroll=ttk.Scrollbar(root,orient="vertical",command=viewport.yview)
        main_scroll.pack(side="right",fill="y");viewport.pack(side="left",fill="both",expand=True)
        viewport.configure(yscrollcommand=main_scroll.set)
        outer=ttk.Frame(viewport,padding=14)
        window=viewport.create_window((0,0),window=outer,anchor="nw")
        outer.bind("<Configure>",lambda event:viewport.configure(scrollregion=viewport.bbox("all")))
        viewport.bind("<Configure>",lambda event:viewport.itemconfigure(window,width=event.width))
        root.bind("<MouseWheel>",lambda event:viewport.yview_scroll(int(-event.delta/120),"units"))
        ttk.Label(outer, text="Exercise results", style="Title.TLabel").pack(anchor="w")
        ttk.Label(outer, text="Part B: wall sit, plank, chest press. Part A: squats and lateral raises.").pack(anchor="w", pady=(1,12))
        top=ttk.Frame(outer);top.pack(fill="x")
        ttk.Label(top, text="Participant label:").pack(side="left")
        self.person=tk.StringVar(value="Participant 1")
        self.people=ttk.Combobox(top, textvariable=self.person, values=self.store.participants(), width=23)
        self.people.pack(side="left",padx=8)
        ttk.Button(top,text="Open profile",command=self.open_profile).pack(side="left")
        ttk.Button(top,text="Manage participants",command=self.manage_participants).pack(side="left",padx=8)
        ttk.Button(top,text="Open results CSV",command=self.open_csv).pack(side="right")
        ttk.Button(top,text="Read Arduino",command=self.read_arduino).pack(side="right",padx=8)
        criteria=ttk.LabelFrame(outer,text="1. Set and confirm criteria",padding=10);criteria.pack(fill="x",pady=12)
        self.wall=tk.StringVar();self.plank=tk.StringVar();self.minimum=tk.StringVar(value="90")
        self.chest=tk.StringVar();self.rep_minimum=tk.StringVar(value="90")
        self.squats=tk.StringVar();self.raises=tk.StringVar()
        fields=[("Wall-sit target (s)",self.wall),("Plank target (s)",self.plank),("Required consistency (%)",self.minimum)]
        for i,(label,var) in enumerate(fields):
            ttk.Label(criteria,text=label).grid(row=0,column=i*2,sticky="w",padx=(0,6))
            ttk.Entry(criteria,textvariable=var,width=8).grid(row=0,column=i*2+1,padx=(0,16))
            var.trace_add("write", self.criteria_changed)
        for column,label,var in ((0,"Chest target (good reps)",self.chest),(2,"Required good reps (%)",self.rep_minimum)):
            ttk.Label(criteria,text=label).grid(row=1,column=column,sticky="w",pady=6)
            ttk.Entry(criteria,textvariable=var,width=8).grid(row=1,column=column+1,sticky="w",padx=(0,16))
            var.trace_add("write",self.criteria_changed)
        for column,label,var in ((0,"Squat target (reps)",self.squats),(2,"Lateral raise target (reps)",self.raises)):
            ttk.Label(criteria,text=label).grid(row=2,column=column,sticky="w",pady=6)
            ttk.Entry(criteria,textvariable=var,width=8).grid(row=2,column=column+1,sticky="w",padx=(0,16))
            var.trace_add("write",self.criteria_changed)
        ttk.Button(criteria,text="Review & confirm",command=self.review_criteria).grid(row=3,column=0,columnspan=2,sticky="w",pady=(8,0))
        self.criteria_note=tk.StringVar(value="Enter targets for loaded exercises. Percentage goals are editable prototype settings.")
        ttk.Label(criteria,textvariable=self.criteria_note).grid(row=3,column=2,columnspan=4,sticky="w",pady=(8,0))
        self.criteria_controls=list(criteria.winfo_children())
        self.criteria_summary=ttk.Label(criteria,textvariable=self.criteria_note,wraplength=720)
        self.criteria_edit=ttk.Button(criteria,text="Change criteria",command=self.edit_criteria)
        self.criteria_summary.grid(row=0,column=0,columnspan=5,sticky="w")
        self.criteria_edit.grid(row=0,column=5,sticky="e")
        self.criteria_summary.grid_remove();self.criteria_edit.grid_remove()
        self.person.trace_add("write",self.participant_changed)
        list_header=ttk.Frame(outer);list_header.pack(fill="x")
        ttk.Label(list_header,text="2. Select an attempt",style="Heading.TLabel").pack(side="left")
        self.visible_rows=tk.StringVar(value="10" if root.winfo_screenheight()>=1000 else "7")
        row_picker=ttk.Combobox(list_header,textvariable=self.visible_rows,values=("7","10"),state="readonly",width=3)
        row_picker.pack(side="right");ttk.Label(list_header,text="Visible rows: ").pack(side="right")
        row_picker.bind("<<ComboboxSelected>>",lambda event:self.table.configure(height=int(self.visible_rows.get())))
        table=ttk.Frame(outer);table.pack(fill="both",expand=True,pady=(7,10))
        columns=("attempt","exercise","hold","consistency","result")
        self.table=ttk.Treeview(table,columns=columns,show="headings",height=int(self.visible_rows.get()),selectmode="browse")
        for key,title,width in zip(columns,("Session","Exercise","Time / reps","Consistency / good %","Result / reason"),(60,105,130,160,325)):
            self.table.heading(key,text=title);self.table.column(key,width=width,anchor="w")
        self.table.pack(side="left",fill="both",expand=True)
        scroll=ttk.Scrollbar(table,orient="vertical",command=self.table.yview);scroll.pack(side="right",fill="y")
        self.table.configure(yscrollcommand=scroll.set)
        self.table.bind("<<TreeviewSelect>>",lambda event:self.show_selected())
        detail=ttk.Frame(outer,padding=(0,6));detail.pack(fill="x")
        self.attempt_title=tk.StringVar(value="No attempt selected")
        ttk.Label(detail,textvariable=self.attempt_title,style="Heading.TLabel").pack(anchor="w",pady=(0,4))
        self.verdict=tk.Label(detail,text="Confirm criteria to evaluate",font=("Segoe UI",13,"bold"),bg="#f3f6fb",fg="#3d5870",anchor="w")
        self.verdict.pack(fill="x")
        self.stats=tk.StringVar(value="Select a session after importing results.")
        ttk.Label(detail,textvariable=self.stats,font=("Segoe UI",11),justify="left",wraplength=780).pack(anchor="w",pady=2)
        self.feedback=tk.StringVar()
        ttk.Label(detail,textvariable=self.feedback,wraplength=780,justify="left").pack(anchor="w")
        self.progress_text=tk.StringVar(value="Progress will compare the same exercise and tolerance under your confirmed criteria.")
        ttk.Label(detail,textvariable=self.progress_text,wraplength=780,justify="left",).pack(anchor="w",pady=(5,0))
        foot=ttk.Frame(outer);foot.pack(before=detail,fill="x",pady=(0,10))
        ttk.Button(foot,text="View progression",command=self.show_progress).pack(side="left")
        ttk.Button(foot,text="Clean report (PDF)",command=self.export_clean_report).pack(side="left",padx=8)
        ttk.Button(foot,text="Export evaluated CSV",command=self.export_report).pack(side="left")
        ttk.Button(foot,text="Delete selected row",command=self.delete_selected_row).pack(side="left",padx=8)
        self.status=tk.StringVar(value="Ready. Local history is kept on this computer; use participant labels to keep people separate.")
        ttk.Label(outer,textvariable=self.status,wraplength=1000).pack(anchor="w",pady=(9,0))
        ttk.Label(outer,text="Results use recorded counts, proximity and duration. Import time is not the session date.",foreground="#526578",wraplength=1000).pack(anchor="w",pady=(4,0))
        root.protocol("WM_DELETE_WINDOW",self.close)
        self.open_profile()
        root.after(100,self.check_jobs)

    def set_criteria_collapsed(self,collapsed):
        for widget in self.criteria_controls:
            widget.grid_remove() if collapsed else widget.grid()
        for widget in (self.criteria_summary,self.criteria_edit):
            widget.grid() if collapsed else widget.grid_remove()

    def edit_criteria(self):
        self.criteria_changed()

    def criteria_changed(self,*args):
        self.confirmed=False
        if hasattr(self,"criteria_controls"):self.set_criteria_collapsed(False)
        if hasattr(self,"criteria_note"):
            self.criteria_note.set("Criteria changed — review and confirm before evaluating.")
        if hasattr(self,"table"): self.refresh_table()

    def participant_changed(self,*args):
        self.confirmed=False
        if hasattr(self,"table"):
            self.rows=[];self.refresh_table()
            self.status.set("Click Open profile to load this participant, or import their results.")

    def criteria(self):
        targets={}
        for key,var in (("wall_sit",self.wall),("plank",self.plank),("chest_press",self.chest),("squats",self.squats),("lateral_raises",self.raises)):
            if not var.get().strip():continue
            try:targets[key]=float(var.get())
            except ValueError:raise ValueError(f"Enter a numeric target for {EXERCISES[key].lower()}.") from None
        try:minimum=float(self.minimum.get());rep_minimum=float(self.rep_minimum.get())
        except ValueError:raise ValueError("Enter numeric percentage goals.") from None
        validate_criteria(targets,minimum,rep_minimum)
        missing={r['exercise'] for r in self.rows}-targets.keys()
        if missing:raise ValueError('Enter a target for '+', '.join(EXERCISES[k] for k in sorted(missing))+'.')
        return targets,minimum,rep_minimum

    def open_profile(self):
        name=self.person.get().strip()
        if not name: return
        saved=self.store.criteria(name)
        if saved and 'targets' in saved and 'minimum' in saved:
            self.wall.set(str(saved["targets"].get("wall_sit","")))
            self.plank.set(str(saved["targets"].get("plank","")))
            self.chest.set(str(saved["targets"].get("chest_press","")))
            self.squats.set(str(saved["targets"].get("squats","")));self.raises.set(str(saved["targets"].get("lateral_raises","")))
            self.rep_minimum.set(str(saved.get("rep_minimum",90)))
            self.minimum.set(str(saved["minimum"]))
        else:
            self.wall.set("");self.plank.set("");self.chest.set("");self.squats.set("");self.raises.set("");self.minimum.set("90");self.rep_minimum.set("90")
        self.confirmed=False
        self.rows=self.store.load(name)
        self.refresh_table()
        self.people.configure(values=self.store.participants())
        self.status.set(f"{name}: {len(self.rows)} saved attempt(s). Confirm criteria to see evaluations.")

    def review_criteria(self):
        if not self.person.get().strip():
            messagebox.showerror("Participant","Enter a participant label first.",parent=self.root);return
        try: targets,minimum,rep_minimum=self.criteria()
        except (ValueError,TypeError) as error:
            messagebox.showerror("Check criteria",str(error),parent=self.root);return
        dialog=tk.Toplevel(self.root);dialog.title("Confirm evaluation criteria");dialog.transient(self.root)
        dialog.geometry("760x520");dialog.grab_set()
        ttk.Label(dialog,text="Please confirm before evaluating",style="Heading.TLabel").pack(anchor="w",padx=18,pady=14)
        body=ttk.Frame(dialog);body.pack(fill="both",expand=True,padx=18,pady=(0,12))
        rules=tk.Text(body,wrap="word",font=("Segoe UI",10),height=15,bg="#f3f6fb",relief="flat")
        scroll=ttk.Scrollbar(body,orient="vertical",command=rules.yview)
        rules.configure(yscrollcommand=scroll.set);rules.pack(side="left",fill="both",expand=True);scroll.pack(side="right",fill="y")
        rules.insert('1.0',criteria_text(targets,minimum,rep_minimum));rules.configure(state='disabled')
        buttons=ttk.Frame(dialog,padding=18);buttons.pack(before=body,side="bottom",fill="x")
        ttk.Button(buttons,text="Back",command=dialog.destroy).pack(side="left")
        def confirm():
            self.accept_criteria(targets,minimum,rep_minimum)
            dialog.destroy()
        ttk.Button(buttons,text="Confirm and evaluate",command=confirm).pack(side="right")

    def accept_criteria(self,targets,minimum,rep_minimum=90):
        self.store.save_criteria(self.person.get().strip(),targets,minimum,rep_minimum)
        self.confirmed=True
        self.set_criteria_collapsed(True)
        self.criteria_note.set("Confirmed: "+", ".join(f"{EXERCISES[k]} {v:g}{' reps' if k=='chest_press' or k in COUNT_EXERCISES else 's'}" for k,v in targets.items())+f". Hold ≥{minimum:g}%; chest ≥{rep_minimum:g}%.")
        self.refresh_table()

    def open_csv(self):
        path=filedialog.askopenfilename(title="Open saved sessions",filetypes=[("CSV results","*.csv")])
        if not path:return
        try:self.preview_import(load_csv(path),str(path))
        except (ValueError,TypeError,OSError,KeyError,csv.Error) as error:messagebox.showerror("Could not read results",str(error),parent=self.root)

    def preview_import(self,rows,source):
        if not rows:messagebox.showinfo("No saved sessions","This source has no saved sessions.",parent=self.root);return
        if not self.person.get().strip():messagebox.showerror("Participant","Enter a participant label first.",parent=self.root);return
        dialog=tk.Toplevel(self.root);dialog.title("Choose sessions to keep");dialog.geometry("850x440");dialog.transient(self.root);dialog.grab_set()
        ttk.Label(dialog,text=f"Keep only {self.person.get().strip()}'s sessions selected. Ctrl-click to select/deselect.",wraplength=800).pack(anchor="w",padx=14,pady=14)
        preview=ttk.Frame(dialog);preview.pack(fill="both",expand=True,padx=14)
        tree=ttk.Treeview(preview,columns=("id","exercise","active","hold","tail","band"),show="headings",selectmode="extended",height=7)
        for key,label in zip(tree["columns"],("Board ID","Exercise","Active (s)","Hold / good reps","Ending / short reps","Zone / travel")):
            tree.heading(key,text=label);tree.column(key,width=125)
        tree.pack(side="left",fill="both",expand=True)
        scrollbar=ttk.Scrollbar(preview,orient="vertical",command=tree.yview)
        scrollbar.pack(side="right",fill="y");tree.configure(yscrollcommand=scrollbar.set)
        for i,row in enumerate(rows):
            tree.insert("","end",iid=str(i),values=preview_values(row))
        tree.selection_set(tree.get_children())
        buttons=ttk.Frame(dialog,padding=14);buttons.pack(fill="x")
        ttk.Button(buttons,text="Cancel",command=dialog.destroy).pack(side="left")
        def keep():
            selected=[rows[int(i)] for i in tree.selection()]
            if not selected: return
            added=self.store.add(self.person.get().strip(),selected)
            self.rows=self.store.load(self.person.get().strip());self.confirmed=False;self.set_criteria_collapsed(False);self.refresh_table()
            self.people.configure(values=self.store.participants())
            self.status.set(f"Added {added} new attempt(s); duplicates skipped. Saved on this computer. Source: {source}")
            dialog.destroy()
        ttk.Button(buttons,text="Keep selected sessions",command=keep).pack(side="right")
        return dialog

    def refresh_table(self):
        selected=self.selected_index()
        self.table.delete(*self.table.get_children())
        criteria=self.criteria() if self.confirmed else None
        for i,row in enumerate(self.rows):
            counts=row['exercise'] in COUNT_EXERCISES
            chest=row['exercise']=='chest_press'
            hold=f"{row['reps']} reps" if counts else f"{row['good_reps']} good reps" if chest else f"{row['in_band_ms']/1000:.1f}s"
            consistency="Not evaluated";status="Confirm criteria"
            if criteria:
                result=evaluate(row,*criteria)
                value=None if counts else result['good_rep_percent'] if chest else result['consistency_percent']
                goal=result['target_reps'] if chest or counts else result['target_seconds']
                hold+=f" / {goal:g}"+(' reps' if chest or counts else 's')
                threshold=criteria[2] if chest else criteria[1]
                consistency="Count only" if counts else "Unavailable" if value is None else f"{value:.1f}% / {threshold:g}%"
                status=result_label(result,threshold)
            self.table.insert("","end",iid=str(i),values=(attempt_number(self.rows,i),EXERCISES[row["exercise"]],hold,consistency,status))
        if self.rows:
            index=selected if selected is not None and selected<len(self.rows) else len(self.rows)-1
            self.table.selection_set(str(index));self.table.see(str(index))
        self.show_selected()

    def selected_index(self):
        values=self.table.selection();return int(values[0]) if values else None

    def show_selected(self):
        index=self.selected_index()
        if index is None:
            self.attempt_title.set("No attempt selected")
            self.verdict.configure(text="Import results to begin",fg="#3d5870")
            self.stats.set("Open a CSV or read the Arduino to add results.");self.feedback.set("")
            self.progress_text.set("");return
        self.table.see(str(index))
        row=self.rows[index];counts=row['exercise'] in COUNT_EXERCISES;chest=row['exercise']=='chest_press'
        context="recorded reps (Part A)" if counts else f"travel {abs(row['pressed_cm']-row['start_cm']):.1f}cm" if chest else f"target zone ±{row['band_cm']:g}cm"
        self.attempt_title.set(f"{EXERCISES[row['exercise']]} - Session {attempt_number(self.rows,index)} - {context}")
        if not self.confirmed:
            self.verdict.configure(text="Confirm criteria first",fg="#3d5870")
            measured=f"{row['reps']} reps" if counts else f"{row['good_reps']} good / {row['attempts']} attempts" if chest else f"{row['in_band_ms']/1000:.1f}s in zone"
            self.stats.set(f"Recorded: {measured} | Active: {row['active_ms']/1000:.1f}s")
            self.feedback.set("Enter targets for the loaded exercises, then Review & confirm.")
            self.progress_text.set("");return
        targets,minimum,rep_minimum=self.criteria();r=evaluate(row,targets,minimum,rep_minimum)
        threshold=rep_minimum if chest else minimum
        color={"Good":"#16734a","Not good yet":"#ae4b1a","Cannot evaluate":"#586b7e"}[r["status"]]
        self.verdict.configure(text=result_label(r,threshold),fg=color)
        if counts:
            bpm='Unavailable' if r['bpm_avg'] is None else f"{r['bpm_avg']:.1f}"
            self.stats.set(f"Recorded reps {r['rep_count']} / goal {r['target_reps']:g} | Active {r['active_seconds']:.1f}s\nMean accepted BPM: {bpm} | Accepted beat samples: {r['bpm_samples']} (informational)")
        elif chest:
            pct='Unavailable' if r['good_rep_percent'] is None else f"{r['good_rep_percent']:.1f}%"
            self.stats.set(f"Good reps {r['good_reps']} / goal {r['target_reps']:g} | {r['short_attempts']} short of {r['attempts']} attempts | Good reps {pct} / goal {rep_minimum:g}%\n"
                           f"Active {r['active_seconds']:.1f}s | Required travel {r['required_travel_cm']:.1f}cm | Return within {r['return_tolerance_cm']:.1f}cm")
        else:
            pct='Unavailable' if r['consistency_percent'] is None else f"{r['consistency_percent']:.1f}%"
            self.stats.set(f"Held {r['in_band_seconds']:.1f}s / goal {r['target_seconds']:g}s | Consistency {pct} / goal {minimum:g}%")
        self.feedback.set(short_feedback(r,threshold))
        change=progression(self.rows,index,targets,minimum,rep_minimum)
        self.progress_text.set(progress_summary(self.rows,index,change))

    def delete_selected_row(self):
        index=self.selected_index()
        if index is None:
            messagebox.showinfo('Select a row','Select the data row you want to delete.',parent=self.root);return
        participant=self.person.get().strip();row=self.rows[index]
        label=f"{EXERCISES[row['exercise']]} - Session {attempt_number(self.rows,index)}"
        if not messagebox.askyesno('Delete selected row?',
                f"Delete {label} from {participant}?\n\nOnly this row is removed from the app. Your participant, criteria and other rows are kept.\nArduino records and CSV files stay unchanged. Re-importing the source can add this row back.",
                parent=self.root,default='no'):return
        count=self.store.delete_row(participant,row)
        self.rows=self.store.load(participant);self.refresh_table()
        self.people.configure(values=self.store.participants())
        self.status.set('Selected row deleted. Participant and other data kept; importing the source can restore the row.' if count else 'That row was already removed.')

    def delete_profile(self, name):
        """Delete the complete local profile. The caller owns user confirmation."""
        count=self.store.delete_participant(name)
        self.people.configure(values=self.store.participants())
        if self.person.get().strip()==name:
            self.person.set("");self.wall.set("");self.plank.set("");self.chest.set("");self.squats.set("");self.raises.set("");self.minimum.set("90");self.rep_minimum.set("90")
            self.rows=[];self.confirmed=False;self.refresh_table()
        self.status.set(f"Deleted {name} and {count} local attempt(s). Exported files and Arduino history were kept.")
        return count

    def manage_participants(self):
        dialog=tk.Toplevel(self.root);dialog.title("Manage participants");dialog.geometry("650x500")
        dialog.transient(self.root);dialog.grab_set()
        body=ttk.Frame(dialog,padding=14);body.pack(fill="both",expand=True)
        ttk.Label(body,text="Search participant labels",style="Heading.TLabel").pack(anchor="w")
        query=tk.StringVar();search=ttk.Entry(body,textvariable=query);search.pack(fill="x",pady=8)
        note=tk.StringVar();ttk.Label(body,textvariable=note).pack(anchor="w",pady=(0,8))
        area=ttk.Frame(body);area.pack(fill="both",expand=True)
        tree=ttk.Treeview(area,columns=("participant","count"),show="headings",height=9,selectmode="browse")
        tree.heading("participant",text="Participant");tree.heading("count",text="Saved attempts")
        tree.column("participant",width=410);tree.column("count",width=120)
        tree.pack(side="left",fill="both",expand=True)
        scroll=ttk.Scrollbar(area,orient="vertical",command=tree.yview);scroll.pack(side="right",fill="y");tree.configure(yscrollcommand=scroll.set)
        visible=[]
        def refresh(*args):
            tree.delete(*tree.get_children());visible[:]=self.store.participant_summaries(query.get(),limit=100)
            for i,row in enumerate(visible):tree.insert("","end",iid=str(i),values=(row['participant'],row['attempts']))
            total=self.store.participant_count()
            note.set(f"{total} saved participant(s). Showing up to 100 matches; type to narrow the list.")
        query.trace_add("write",refresh)
        def selected():
            ids=tree.selection();return visible[int(ids[0])] if ids else None
        def open_selected():
            row=selected()
            if row:self.person.set(row['participant']);self.open_profile();dialog.destroy()
        def delete_selected():
            row=selected()
            if not row:return
            if messagebox.askyesno("Delete participant?",f"Delete {row['participant']}?\n\nThis removes the profile, saved criteria and all {row['attempts']} local attempt(s). This cannot be undone.\nExported files and Arduino records stay untouched.",parent=dialog,default="no"):
                self.delete_profile(row['participant']);refresh()
        buttons=ttk.Frame(body);buttons.pack(fill="x",pady=(12,0))
        ttk.Button(buttons,text="Delete participant",command=delete_selected).pack(side="left")
        ttk.Button(buttons,text="Open selected",command=open_selected).pack(side="right")
        ttk.Button(buttons,text="Close",command=dialog.destroy).pack(side="right",padx=8)
        tree.bind("<Double-1>",lambda event:open_selected())
        refresh();search.focus_set()
        return dialog

    def export_clean_report(self):
        if not self.confirmed:
            messagebox.showinfo("Confirm criteria","Review and confirm criteria before creating a report.",parent=self.root);return
        if not self.rows:
            messagebox.showinfo("No attempts","Import saved attempts first.",parent=self.root);return
        path=filedialog.asksaveasfilename(title="Save readable progress report",defaultextension=".pdf",initialfile="exercise_progress.pdf",filetypes=[("PDF report","*.pdf")])
        if not path:return
        try:
            build_report(path,self.person.get().strip(),self.rows,*self.criteria())
            self.status.set(f"PDF report saved: {path}")
            webbrowser.open(Path(path).resolve().as_uri())
        except FileExistsError:messagebox.showerror("File exists","Choose a new filename; existing reports are preserved.",parent=self.root)
        except (OSError,ValueError) as error:messagebox.showerror("Report failed",str(error),parent=self.root)

    def show_progress(self):
        if not self.confirmed:messagebox.showinfo("Confirm criteria","Review and confirm criteria first.",parent=self.root);return
        index=self.selected_index()
        if index is None:return
        current=self.rows[index];targets,minimum,rep_minimum=self.criteria()
        counts=current['exercise'] in COUNT_EXERCISES
        chest=current['exercise']=='chest_press'
        rows=[r for r in self.rows if r['exercise']==current['exercise'] and (chest or counts or abs(r['band_cm']-current['band_cm'])<0.001)]
        scored=[(row,evaluate(row,targets,minimum,rep_minimum)) for row in rows]
        metric="rep_count" if counts else "good_rep_percent" if chest else "consistency_percent"
        requirement=targets[current["exercise"]] if counts else rep_minimum if chest else minimum
        scored=[(row,r) for row,r in scored if r[metric] is not None]
        dialog=tk.Toplevel(self.root);dialog.title("Progression — "+EXERCISES[current['exercise']]);dialog.geometry("900x520")
        ttk.Label(dialog,text=f"{self.person.get()} · {EXERCISES[current['exercise']]}",style="Heading.TLabel").pack(anchor="w",padx=16,pady=10)
        ttk.Label(dialog,text="Part A: recorded counts only. Chest travel may differ; see the PDF for details.").pack(anchor="w",padx=16)
        canvas=tk.Canvas(dialog,bg="white",highlightthickness=0);canvas.pack(fill="both",expand=True,padx=16,pady=12)
        def draw(event=None):
            canvas.delete("all");w=max(canvas.winfo_width(),800);h=max(canvas.winfo_height(),350)
            if not scored:canvas.create_text(w/2,h/2,text="No scored sessions to plot yet.",font=("Segoe UI",13));return
            left,right,top,bottom=65,w-35,30,h-70
            canvas.create_line(left,top,left,bottom,right,bottom,fill="#9aa8b5")
            canvas.create_text(left,15,text="Recorded reps" if counts else "Good reps (%)" if chest else "Consistency (%)",anchor="w",fill="#24445e")
            maximum=max([requirement]+[r[metric] for _,r in scored]+[1])*1.1 if counts else 100
            for fraction in (0,.25,.5,.75,1):
                value=maximum*fraction
                y=bottom-(bottom-top)*fraction;canvas.create_line(left,y,right,y,fill="#e5ebf0");canvas.create_text(left-10,y,text=f"{value:.0f}",anchor="e")
            threshold=bottom-(bottom-top)*requirement/maximum
            canvas.create_line(left,threshold,right,threshold,fill="#cf9f39",dash=(5,4))
            pts=[]
            for i,(row,r) in enumerate(scored):
                x=left+(right-left)*(i/(len(scored)-1) if len(scored)>1 else 0.5)
                y=bottom-(bottom-top)*r[metric]/maximum;pts.extend((x,y))
                canvas.create_oval(x-4,y-4,x+4,y+4,fill="#176eaa",outline="")
                if len(scored)<=12:canvas.create_text(x,bottom+17,text=f"{i+1}")
            if len(pts)>2:canvas.create_line(*pts,fill="#176eaa",width=2)
            canvas.create_text((left+right)/2,h-20,text="Session order · dashed line is your confirmed goal",fill="#3e576b")
        canvas.bind("<Configure>",draw);dialog.after(100,draw)

    def export_report(self):
        if not self.confirmed:messagebox.showinfo("Confirm criteria","Confirm criteria before exporting an assessment.",parent=self.root);return
        if not self.rows:return
        path=filedialog.asksaveasfilename(title="Save assessment report",defaultextension=".csv",initialfile="exercise_results.csv",filetypes=[("CSV","*.csv")])
        if not path:return
        if Path(path).exists():messagebox.showerror("File exists","Choose a new filename; existing reports are preserved.",parent=self.root);return
        targets,minimum,rep_minimum=self.criteria()
        try:
            export_assessment_csv(path,self.person.get().strip(),self.rows,targets,minimum,rep_minimum)
            self.status.set(f"Evaluated CSV saved: {path}")
        except (OSError,ValueError) as error:messagebox.showerror("Save failed",str(error),parent=self.root)

    def read_arduino(self):
        if self.busy:return
        try:
            import serial.tools.list_ports
            ports=list(serial.tools.list_ports.comports())
        except ImportError:messagebox.showerror("Serial support","The source version needs pyserial. The Windows executable includes it.",parent=self.root);return
        dialog=tk.Toplevel(self.root);dialog.title("Read saved Arduino sessions");dialog.geometry("570x250");dialog.transient(self.root);dialog.grab_set()
        ttk.Label(dialog,text="Finish and save the session first. Close Arduino Serial Monitor.\nConnecting may reset the Uno; saved summaries remain.",wraplength=530).pack(anchor="w",padx=16,pady=16)
        port=tk.StringVar(value=ports[0].device if ports else "COM10")
        box=ttk.Combobox(dialog,textvariable=port,values=[p.device for p in ports]);box.pack(anchor="w",padx=16)
        ttk.Label(dialog,text="Select either board's port. Part A or B is detected automatically.").pack(anchor="w",padx=16,pady=8)
        def read():
            device=port.get().strip()
            if not device:return
            dialog.destroy();self.busy=True;self.status.set("Detecting board and reading saved sessions…")
            def worker():
                try:
                    import serial
                    with serial.Serial(device,115200,timeout=0.25) as connection:
                        time.sleep(2);connection.reset_input_buffer();rows,notice=read_all_history(connection)
                    self.jobs.put(("rows",rows,device+(" - "+notice if notice else "")))
                except Exception as error:self.jobs.put(("error",str(error),device))
            threading.Thread(target=worker,daemon=True).start()
        ttk.Button(dialog,text="Read saved results",command=read).pack(anchor="e",padx=16,pady=12)

    def check_jobs(self):
        try:
            kind,value,source=self.jobs.get_nowait();self.busy=False
            if kind=="error":
                self.status.set("Could not read the board.")
                messagebox.showerror("Read failed",str(value)+"\nClose Serial Monitor, check the port and retry.",parent=self.root)
            else:self.preview_import(value,source)
        except queue.Empty:pass
        self.root.after(100,self.check_jobs)

    def close(self):
        self.store.close();self.root.destroy()


def smoke_test():
    with tempfile.TemporaryDirectory() as directory:
        root=tk.Tk();root.withdraw();app=ResultsApp(root,Path(directory)/"test.sqlite3")
        holds=[]
        for i,(exercise,inside) in enumerate([("wall_sit",18000),("plank",18000),("wall_sit",26000),
                ("plank",27000),("wall_sit",28000),("plank",26000),("wall_sit",29000),
                ("plank",28000),("wall_sit",29500),("plank",24000)],1):
            holds.append(normalize(dict(session_id=i,exercise=exercise,active_ms=35000,in_band_ms=inside,
                reference_cm=50,band_cm=10,trailing_out_ms=5000,has_hold=1,history_id=1)))
        # Start with a pre-update profile, then import new chest records into it.
        app.store.save_criteria('Empty after deletion',{'squats':5},90)
        with app.store.db:
            app.store.db.execute('UPDATE settings SET payload=? WHERE participant=?',('{}','Empty after deletion'))
        app.person.set('Empty after deletion');app.open_profile();assert not app.rows
        app.store.delete_participant('Empty after deletion');app.person.set('Participant 1')
        app.store.add("Participant 1",holds)
        app.store.save_criteria("Participant 1",{'wall_sit':25,'plank':25},90)
        app.open_profile();assert app.chest.get()=='' and len(app.rows)==10
        chest=[normalize(dict(session_id=i,exercise='chest_press',active_ms=35000,
               good_reps=good,attempts=attempts,start_cm=20,pressed_cm=end,last_travel_cm=40,history_id=1))
               for i,good,attempts,end in ((1,6,10,50),(2,9,12,54.2),(3,12,12,54.2))]
        app.store.add("Participant 1",chest);app.rows=app.store.load("Participant 1")
        app.chest.set('10');app.rep_minimum.set('90');app.accept_criteria(*app.criteria());root.update()
        assert len(app.table.get_children())==13 and int(app.table.cget('height'))>=7
        good_index=next(i for i,r in enumerate(app.rows) if r['exercise']=='chest_press' and r['session_id']==3)
        app.table.selection_set(str(good_index));app.show_selected();root.update()
        assert app.verdict.cget('text')=='Good - both goals met'
        chest_index=next(i for i,r in enumerate(app.rows) if r['exercise']=='chest_press' and r['session_id']==2)
        app.table.selection_set(str(chest_index));app.show_selected();root.update()
        assert 'rep count + good-rep %' in app.verdict.cget('text')
        assert 'Chest press - Session 2' in app.attempt_title.get()
        assert '75.0%' in app.stats.get() and '3 short' in app.stats.get()
        assert 'travel changed' in app.progress_text.get()
        popup=app.preview_import(chest,'Synthetic CSV');root.update();popup.destroy()
        app.show_progress();root.update()
        for w in root.winfo_children():
            if isinstance(w,tk.Toplevel):w.destroy()
        # Import Part A into the same pre-existing B profile. No migration or
        # profile reset is needed; optional BPM and zero counts remain distinct.
        assert app.squats.get()=='' and app.raises.get()==''
        part_a=[normalize(dict(session_id=i,exercise=exercise,active_ms=20000+i*1000,
                    reps=reps,bpm_avg=bpm,bpm_samples=4 if bpm else 0,history_id=1))
                for exercise in COUNT_EXERCISES
                for i,reps,bpm in ((1,2,None),(2,5,72.3),(3,8,75.1))]
        popup=app.preview_import(part_a,'Synthetic Part A CSV');root.update();popup.destroy()
        app.store.add('Participant 1',part_a);app.rows=app.store.load('Participant 1')
        app.squats.set('5');app.raises.set('8');app.accept_criteria(*app.criteria());root.update()
        assert len(app.table.get_children())==19
        for exercise in COUNT_EXERCISES:
            index=next(i for i,r in enumerate(app.rows) if r['exercise']==exercise and r['session_id']==1)
            app.table.selection_set(str(index));app.show_selected();root.update()
            assert app.verdict.cget('text')=='Not good yet - rep count'
            assert 'Unavailable' in app.stats.get() and 'form quality is not measured' in app.feedback.get()
            index=next(i for i,r in enumerate(app.rows) if r['exercise']==exercise and r['session_id']==3)
            app.table.selection_set(str(index));app.show_selected();root.update()
            assert app.verdict.cget('text')=='Good - rep target met' and '75.1' in app.stats.get()
            assert app.table.item(str(index),'values')[3]=='Count only'
            app.show_progress();root.after(150);root.update()
            for w in root.winfo_children():
                if isinstance(w,tk.Toplevel):w.destroy()
        # A transient dialog follows the parent window's mapped state. Show
        # the parent before checking that the confirmation controls are visible.
        root.deiconify();root.update();app.review_criteria();root.update()
        review=next(w for w in root.winfo_children() if isinstance(w,tk.Toplevel))
        assert review.title()=='Confirm evaluation criteria'
        buttons=[w for body in review.winfo_children() for w in body.winfo_children() if isinstance(w,ttk.Button)]
        assert any(w.cget('text')=='Confirm and evaluate' and w.winfo_ismapped() for w in buttons)
        next(w for w in buttons if w.cget('text')=='Confirm and evaluate').invoke();root.update()
        assert app.confirmed and not review.winfo_exists()
        report=Path('clean_report_preview.pdf') if os.environ.get('ECE198_CAPTURE_PREVIEW') else Path(directory)/'report.pdf'
        if report.exists():report.unlink()  # Synthetic output only.
        build_report(report,'Participant 1',app.rows,*app.criteria())
        assert report.stat().st_size>3000
        csv_path=Path(directory)/'assessed.csv'
        export_assessment_csv(csv_path,'Participant 1',app.rows,*app.criteria())
        assert len(load_csv(csv_path))==19
        manager=app.manage_participants();root.update();manager.destroy()
        if os.environ.get('ECE198_CAPTURE_PREVIEW'):
            root.deiconify();root.update();root.after(500);root.update()
            from PIL import ImageGrab
            x,y=root.winfo_rootx(),root.winfo_rooty()
            ImageGrab.grab(bbox=(x,y,x+root.winfo_width(),y+root.winfo_height())).save('app_preview.png')
        # Exercise the real row-deletion handler: decline leaves data intact;
        # confirmation removes just the selected row and preserves criteria.
        before=len(app.rows);key=app.rows[app.selected_index()]['_record_key']
        old_ask=messagebox.askyesno
        try:
            messagebox.askyesno=lambda *args,**kwargs:False
            app.delete_selected_row();assert len(app.rows)==before
            messagebox.askyesno=lambda *args,**kwargs:True
            app.delete_selected_row();root.update()
        finally:messagebox.askyesno=old_ask
        assert len(app.rows)==before-1 and all(r['_record_key']!=key for r in app.rows)
        assert app.store.criteria('Participant 1')['targets']['chest_press']==10
        assert 'Participant 1' in app.store.participants()
        # Regression: hold selection, scoring and graph still work.
        index=next(i for i,r in enumerate(app.rows) if r['exercise']=='wall_sit' and r['session_id']==9)
        app.table.selection_set(str(index));app.show_selected();root.update()
        assert app.verdict.cget('text')=='Good - both goals met'
        assert 'held' in app.stats.get().lower()
        app.show_progress();root.update()
        app.delete_profile('Participant 1');assert not app.rows and not app.store.participants()
        app.close()


if __name__=="__main__":
    if "--smoke-test" in sys.argv:smoke_test()
    else:
        root=tk.Tk()
        try:ResultsApp(root)
        except Exception as error:
            messagebox.showerror("Could not start Hold Results",str(error),parent=root)
            root.destroy();raise SystemExit(1)
        root.mainloop()
