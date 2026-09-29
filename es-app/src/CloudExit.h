#pragma once

// The exit codes the cloud scripts -- cloud_backup, cloud_restore,
// cloud_content_backup, cloud_content_restore -- reserve for a run that did
// nothing, and that EmulationStation names rather than reporting as FAILED.
// The scripts hold the other half of this contract (take_cloud_lock and
// check_network_link in each); the two sides change together and ship in
// one image.
//
// Why these values. The sentinels were 3 and 4, which are also rclone's own
// exit codes for "directory not found" and "file not found": a restore
// against a cloud with no Saves folder yet exited 3 from rclone and the
// transfer page said SKIPPED - ANOTHER CLOUD SYNC IS RUNNING over the files
// it had just restored (fork #99). rclone's codes run 1-9, so the sentinels
// come from sysexits.h instead, where nothing rclone returns can reach them.
// 130 is what the scripts' trap exits with when somebody stops them, and
// what a cancelled startup wait is reported as; it is here so every reader
// spells the same thing the same way.
namespace CloudExit
{
	constexpr int LockHeld  = 75;   // EX_TEMPFAIL: another cloud sync holds the flock
	constexpr int NoNetwork = 69;   // EX_UNAVAILABLE: no default route, or no probe answered
	constexpr int Stopped   = 130;  // the scripts' SIGINT/SIGTERM trap; a cancelled run
}
