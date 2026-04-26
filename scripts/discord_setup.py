#!/usr/bin/env python3
"""
SpectraSynq K1 Discord Server Setup Script

Programmatically configures a Discord server for the K1 hardware product
community. Designed to be run ONCE by the server owner.

Usage:
    python discord_setup.py <BOT_TOKEN>
    BOT_TOKEN=... python discord_setup.py

Requires: pip install discord.py
"""

import os
import sys
from typing import Optional

import discord
from discord import (
    CategoryChannel,
    Guild,
    PermissionOverwrite,
    TextChannel,
)


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

ROLES_SPEC = [
    # (name, colour_hex, hoisted)
    ("Founder", 0xFFB800, True),
    ("Beta-Tester", 0x9B59B6, True),
    ("Contributor", 0x2ECC71, True),
    ("Member", 0x7289DA, False),
]

# Messages to post in specific channels after creation.
RULES_MESSAGE = (
    "**Welcome to SpectraSynq K1**\n"
    "\n"
    "**1.** Be respectful. Criticism of the product is welcome; personal attacks are not.\n"
    "**2.** Stay on topic. This is a music visualisation and ambient light community.\n"
    "**3.** No spam, self-promotion, or affiliate links without permission.\n"
    "**4.** Bug reports go in #troubleshooting with steps to reproduce.\n"
    "**5.** Feature requests: describe the problem you're solving, not just the solution you want.\n"
    "**6.** Share your setup! We love seeing K1 in the wild — post in #show-your-setup.\n"
    "**7.** Founders Edition members: DM a moderator with your order number for the @Founder role.\n"
    "\n"
    '*"Music. Made visible."*'
)

INTRODUCTIONS_MESSAGE = (
    "**Welcome! Tell us about yourself.**\n"
    "\n"
    "Drop an intro \u2014 we\u2019d love to know:\n"
    "\U0001f3b5 What music do you listen to or make?\n"
    "\U0001f527 What\u2019s your current setup?\n"
    "\u2728 What drew you to K1?\n"
    "\n"
    "No pressure on format \u2014 just say hello."
)

ANNOUNCEMENTS_MESSAGE = (
    "**SpectraSynq K1 Community \u2014 Live**\n"
    "\n"
    "This is the official community for K1 owners, backers, and anyone "
    "interested in music visualisation done right.\n"
    "\n"
    "Firmware updates, effect releases, and project news will be posted here.\n"
    "\n"
    "Stay tuned."
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def get_token() -> str:
    """Retrieve the bot token from CLI args or environment variable."""
    if len(sys.argv) > 1:
        return sys.argv[1]
    token = os.environ.get("BOT_TOKEN")
    if token:
        return token
    print("ERROR: No bot token provided.")
    print("Usage: python discord_setup.py <BOT_TOKEN>")
    print("   or: BOT_TOKEN=... python discord_setup.py")
    sys.exit(1)


def _find_role(guild: Guild, name: str) -> Optional[discord.Role]:
    """Find an existing role by name (case-insensitive)."""
    lower = name.lower()
    for role in guild.roles:
        if role.name.lower() == lower:
            return role
    return None


def _find_category(guild: Guild, name: str) -> Optional[CategoryChannel]:
    """Find an existing category by name (case-insensitive)."""
    lower = name.lower()
    for cat in guild.categories:
        if cat.name.lower() == lower:
            return cat
    return None


def _find_channel(guild: Guild, name: str) -> Optional[TextChannel]:
    """Find an existing text or voice channel by name (case-insensitive)."""
    lower = name.lower()
    for ch in guild.channels:
        if ch.name.lower() == lower:
            return ch
    return None


# ---------------------------------------------------------------------------
# Setup logic
# ---------------------------------------------------------------------------

async def setup_server(guild: Guild, bot_user: discord.User) -> None:
    """
    Perform the full server configuration.

    Creates roles, categories, channels, posts initial messages,
    and generates a permanent invite link.
    """
    summary: list[str] = []

    # ------------------------------------------------------------------
    # Step 1: Delete all existing default channels
    # ------------------------------------------------------------------
    print("\n--- Step 1: Deleting existing channels ---")
    for channel in list(guild.channels):
        try:
            await channel.delete(reason="SpectraSynq setup: clearing defaults")
            print(f"  Deleted channel: #{channel.name}")
        except discord.Forbidden:
            print(f"  WARNING: Cannot delete #{channel.name} — insufficient permissions")
        except discord.HTTPException as exc:
            print(f"  WARNING: Failed to delete #{channel.name}: {exc}")
    summary.append("Deleted all existing default channels")

    # ------------------------------------------------------------------
    # Step 2: Configure @everyone — disable send_messages server-wide
    # ------------------------------------------------------------------
    print("\n--- Step 2: Configuring @everyone permissions ---")
    everyone_role = guild.default_role
    perms = everyone_role.permissions
    perms.update(send_messages=False)
    try:
        await everyone_role.edit(permissions=perms, reason="SpectraSynq setup: restrict @everyone")
        print("  @everyone: send_messages disabled server-wide")
        summary.append("@everyone: send_messages disabled")
    except discord.Forbidden:
        print("  WARNING: Cannot edit @everyone permissions — bot needs Manage Roles above @everyone")

    # ------------------------------------------------------------------
    # Step 3: Create roles (top = highest position)
    # ------------------------------------------------------------------
    print("\n--- Step 3: Creating roles ---")
    created_roles: dict[str, discord.Role] = {}

    for role_name, colour_hex, hoisted in ROLES_SPEC:
        existing = _find_role(guild, role_name)
        if existing:
            print(f"  Role '{role_name}' already exists — skipping creation")
            created_roles[role_name] = existing
        else:
            role = await guild.create_role(
                name=role_name,
                colour=discord.Colour(colour_hex),
                hoist=hoisted,
                mentionable=True,
                reason=f"SpectraSynq setup: create {role_name} role",
            )
            created_roles[role_name] = role
            print(f"  Created role: {role_name} (colour #{colour_hex:06X}, hoisted={hoisted})")

    # Reorder roles so Founder is highest, Member is lowest of the custom roles.
    # Role positions: higher number = higher in the hierarchy.
    # We place them just below the bot's highest role.
    bot_member = guild.get_member(bot_user.id)
    if bot_member and bot_member.top_role:
        bot_top_pos = bot_member.top_role.position
    else:
        bot_top_pos = len(guild.roles) - 1

    # Build position mapping: Founder just below bot, then descending.
    role_positions = {}
    for idx, (role_name, _, _) in enumerate(ROLES_SPEC):
        target_pos = max(1, bot_top_pos - 1 - idx)
        role_positions[created_roles[role_name]] = target_pos

    try:
        await guild.edit_role_positions(positions=role_positions)
        print("  Role positions reordered (Founder highest, Member lowest)")
    except discord.Forbidden:
        print("  WARNING: Cannot reorder roles — bot needs Manage Roles permission")
    except discord.HTTPException as exc:
        print(f"  WARNING: Role reorder failed: {exc}")

    summary.append(f"Created {len(created_roles)} roles: {', '.join(created_roles.keys())}")

    # Convenience references for permission overwrites.
    founder_role = created_roles["Founder"]
    beta_role = created_roles["Beta-Tester"]
    contributor_role = created_roles["Contributor"]
    member_role = created_roles["Member"]

    # ------------------------------------------------------------------
    # Step 4: Create categories and channels
    # ------------------------------------------------------------------
    print("\n--- Step 4: Creating categories and channels ---")

    # ---- WELCOME category ----
    print("  Creating category: WELCOME")
    welcome_cat = _find_category(guild, "WELCOME")
    if not welcome_cat:
        welcome_cat = await guild.create_category(
            "WELCOME",
            reason="SpectraSynq setup: WELCOME category",
        )
    summary.append("Created category: WELCOME")

    # #rules — read-only for everyone
    rules_overwrites = {
        everyone_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
        member_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
    }
    rules_ch = await _create_text_channel(
        guild, "rules", welcome_cat, rules_overwrites, summary
    )

    # #announcements — news channel type if possible, read-only
    announcements_overwrites = {
        everyone_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
        member_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
    }
    announcements_ch = await _create_text_channel(
        guild,
        "announcements",
        welcome_cat,
        announcements_overwrites,
        summary,
        news=True,
    )

    # #introductions — @Member+ can send
    intros_overwrites = {
        everyone_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
        member_role: PermissionOverwrite(
            send_messages=True,
            read_messages=True,
        ),
    }
    intros_ch = await _create_text_channel(
        guild, "introductions", welcome_cat, intros_overwrites, summary
    )

    # ---- COMMUNITY category ----
    print("  Creating category: COMMUNITY")
    community_cat = _find_category(guild, "COMMUNITY")
    if not community_cat:
        community_cat = await guild.create_category(
            "COMMUNITY",
            reason="SpectraSynq setup: COMMUNITY category",
        )
    summary.append("Created category: COMMUNITY")

    # @Member+ can send — base overwrite for COMMUNITY channels.
    community_base = {
        everyone_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
        member_role: PermissionOverwrite(
            send_messages=True,
            read_messages=True,
        ),
    }

    general_ch = await _create_text_channel(
        guild, "general", community_cat, community_base, summary
    )

    # #show-your-setup — attach files explicitly allowed
    setup_overwrites = {
        everyone_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
        member_role: PermissionOverwrite(
            send_messages=True,
            read_messages=True,
            attach_files=True,
        ),
    }
    await _create_text_channel(
        guild, "show-your-setup", community_cat, setup_overwrites, summary
    )

    # #troubleshooting — create threads allowed
    troubleshooting_overwrites = {
        everyone_role: PermissionOverwrite(
            send_messages=False,
            read_messages=True,
        ),
        member_role: PermissionOverwrite(
            send_messages=True,
            read_messages=True,
            create_public_threads=True,
            send_messages_in_threads=True,
        ),
    }
    await _create_text_channel(
        guild, "troubleshooting", community_cat, troubleshooting_overwrites, summary
    )

    # #firmware-updates
    await _create_text_channel(
        guild, "firmware-updates", community_cat, community_base, summary
    )

    # #events
    await _create_text_channel(
        guild, "events", community_cat, community_base, summary
    )

    # Voice channel: "Show and Tell"
    existing_vc = _find_channel(guild, "Show and Tell")
    if not existing_vc:
        await guild.create_voice_channel(
            "Show and Tell",
            category=community_cat,
            reason="SpectraSynq setup: Show and Tell voice channel",
        )
        print("  Created voice channel: Show and Tell")
        summary.append("Created voice channel: Show and Tell")
    else:
        print("  Voice channel 'Show and Tell' already exists — skipping")

    # ---- FOUNDERS EDITION category (private) ----
    print("  Creating category: FOUNDERS EDITION")
    founders_cat_overwrites = {
        everyone_role: PermissionOverwrite(
            read_messages=False,
            send_messages=False,
        ),
    }
    founders_cat = _find_category(guild, "FOUNDERS EDITION")
    if not founders_cat:
        founders_cat = await guild.create_category(
            "FOUNDERS EDITION",
            overwrites=founders_cat_overwrites,
            reason="SpectraSynq setup: FOUNDERS EDITION category (private)",
        )
    summary.append("Created category: FOUNDERS EDITION (private)")

    # #founders-lounge — only @Founder
    founders_lounge_overwrites = {
        everyone_role: PermissionOverwrite(
            read_messages=False,
        ),
        founder_role: PermissionOverwrite(
            read_messages=True,
            send_messages=True,
        ),
    }
    await _create_text_channel(
        guild, "founders-lounge", founders_cat, founders_lounge_overwrites, summary
    )

    # #beta-testing — only @Beta-Tester
    beta_overwrites = {
        everyone_role: PermissionOverwrite(
            read_messages=False,
        ),
        beta_role: PermissionOverwrite(
            read_messages=True,
            send_messages=True,
        ),
    }
    await _create_text_channel(
        guild, "beta-testing", founders_cat, beta_overwrites, summary
    )

    # #dev-discussion — only @Contributor
    dev_overwrites = {
        everyone_role: PermissionOverwrite(
            read_messages=False,
        ),
        contributor_role: PermissionOverwrite(
            read_messages=True,
            send_messages=True,
        ),
    }
    await _create_text_channel(
        guild, "dev-discussion", founders_cat, dev_overwrites, summary
    )

    # ------------------------------------------------------------------
    # Step 5: Post initial messages
    # ------------------------------------------------------------------
    print("\n--- Step 5: Posting initial messages ---")

    if rules_ch:
        await rules_ch.send(RULES_MESSAGE)
        print("  Posted rules message in #rules")
        summary.append("Posted rules message")

    if intros_ch:
        await intros_ch.send(INTRODUCTIONS_MESSAGE)
        print("  Posted introduction prompt in #introductions")
        summary.append("Posted introduction prompt")

    if announcements_ch:
        await announcements_ch.send(ANNOUNCEMENTS_MESSAGE)
        print("  Posted launch announcement in #announcements")
        summary.append("Posted launch announcement")

    # ------------------------------------------------------------------
    # Step 6: Create permanent invite link
    # ------------------------------------------------------------------
    print("\n--- Step 6: Creating permanent invite link ---")
    invite_url = None
    if general_ch:
        try:
            invite = await general_ch.create_invite(
                max_age=0,       # Permanent (never expires)
                max_uses=0,      # Unlimited uses
                unique=True,
                reason="SpectraSynq setup: permanent community invite",
            )
            invite_url = str(invite)
            print(f"  Permanent invite: {invite_url}")
            summary.append(f"Permanent invite: {invite_url}")
        except discord.Forbidden:
            print("  WARNING: Cannot create invite — bot needs Create Instant Invite permission")
        except discord.HTTPException as exc:
            print(f"  WARNING: Invite creation failed: {exc}")

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------
    print("\n" + "=" * 60)
    print("SETUP COMPLETE")
    print("=" * 60)
    for item in summary:
        print(f"  - {item}")
    print("=" * 60)

    if invite_url:
        print(f"\nPermanent invite URL: {invite_url}\n")
    else:
        print("\nWARNING: No invite URL generated. Create one manually.\n")


async def _create_text_channel(
    guild: Guild,
    name: str,
    category: CategoryChannel,
    overwrites: dict,
    summary: list[str],
    news: bool = False,
) -> Optional[TextChannel]:
    """
    Create a text channel under the given category with permission overwrites.

    If *news* is True, attempt to create as an announcement (news) channel.
    Returns the channel object or None if creation failed.
    """
    existing = _find_channel(guild, name)
    if existing:
        print(f"  Channel #{name} already exists — skipping")
        return existing

    kwargs = {
        "name": name,
        "category": category,
        "overwrites": overwrites,
        "reason": f"SpectraSynq setup: #{name}",
    }

    channel = await guild.create_text_channel(**kwargs)

    if news:
        # Attempt to convert to announcement/news channel.  Requires the
        # guild to have the Community feature enabled.  If it fails, the
        # channel remains a regular text channel — perfectly functional.
        try:
            await channel.edit(type=discord.ChannelType.news)
            print(f"  Created news channel: #{name}")
            summary.append(f"Created news channel: #{name}")
            return channel
        except (discord.HTTPException, TypeError):
            print(
                f"  NOTE: Cannot set #{name} as news channel "
                f"(Community feature may not be enabled). "
                f"Created as regular text channel instead."
            )

    print(f"  Created text channel: #{name}")
    summary.append(f"Created text channel: #{name}")
    return channel


# ---------------------------------------------------------------------------
# Bot client
# ---------------------------------------------------------------------------

class SetupBot(discord.Client):
    """
    Minimal bot client that runs the server setup on ready, then disconnects.
    """

    def __init__(self, **kwargs):
        intents = discord.Intents.default()
        intents.guilds = True
        intents.members = True
        super().__init__(intents=intents, **kwargs)

    async def on_ready(self):
        print(f"Bot connected as {self.user} (ID: {self.user.id})")

        if not self.guilds:
            print("ERROR: Bot is not in any guild. Invite it to a server first.")
            await self.close()
            return

        guild = self.guilds[0]
        print(f"Target guild: {guild.name} (ID: {guild.id})")

        try:
            await setup_server(guild, self.user)
        except discord.Forbidden as exc:
            print(f"\nERROR: Insufficient permissions — {exc}")
            print("Ensure the bot has Administrator permission in the target guild.")
        except discord.HTTPException as exc:
            print(f"\nERROR: Discord API error — {exc}")
        except Exception as exc:
            print(f"\nERROR: Unexpected failure — {type(exc).__name__}: {exc}")
            raise
        finally:
            await self.close()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    token = get_token()
    print("SpectraSynq K1 Discord Server Setup")
    print("=" * 40)
    print("Connecting to Discord...")

    bot = SetupBot()

    try:
        bot.run(token, log_handler=None)
    except discord.LoginFailure:
        print("ERROR: Invalid bot token. Check your token and try again.")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nSetup interrupted by user.")
        sys.exit(1)


if __name__ == "__main__":
    main()
