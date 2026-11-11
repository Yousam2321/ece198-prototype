# Sharing this Git repository

## Local repository supplied in the ZIP

Extract the ZIP. The ece198-prototype folder includes its .git directory and main branch.
Keep that directory if you want the included commit history. Check it with:

```sh
cd ece198-prototype
git status
git log --oneline
```

The .gitignore excludes test CSV files, builds and credentials. Review any files before
adding them. Commit authorship for the supplied initial commit is Codex; each teammate
should configure their own Git name/email before making subsequent commits.

## Online repository

**https://github.com/Yousam2321/ece198-prototype** is the private team repository.
Clone the online repository for a checkout that matches its published history.
The earlier downloadable ZIP contains the local preparation commit; clone from
GitHub for collaboration rather than pushing that ZIP's separate history.

The owner can add teammates through Settings > Collaborators. Never put passwords
or tokens in repository files. No license has been chosen automatically.

## Teammate workflow

```sh
git clone https://github.com/Yousam2321/ece198-prototype.git
cd ece198-prototype
git switch -c person2/station-tests
# Edit your assigned files, compile and test.
git add HoldStation docs
git commit -m "Document station calibration results"
git push -u origin person2/station-tests
```

Open a pull request and have someone from the other pair review it before merging.
Use separate branches per task; coordinate before editing shared PrototypeCore.h.
There are two identical copies of that header for Arduino IDE compatibility. When
changing shared logic, update both and run python tests/run_tests.py.

## Suggested first branches

| Owner | Branch | Primary paths |
|---|---|---|
| Person 1 | person1/station-hardware | docs/HARDWARE.md |
| Person 2 | person2/station-tests | HoldStation/ and tests/ |
| Person 3 | person3/wearable-hardware | docs/HARDWARE.md |
| Person 4 | person4/wearable-tests | WearableMonitor/ and tests/ |

Persons 1 and 3 should coordinate edits to the hardware document or keep measurements
in separate new Markdown files. Do not commit identifiable participant data.
