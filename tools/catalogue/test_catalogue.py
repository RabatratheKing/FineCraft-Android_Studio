import hashlib
import unittest
from generate_catalogue import (BEHAVIOUR_TAG_CONFIDENCE, GEOMETRY_CLASSES, SOURCE_GIT_BLOBS,
                                build, canonical_file_bytes, canonical_lf_bytes, git_blob_sha1, DEFAULT_SOURCE)

class CatalogueSchemaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs = build(DEFAULT_SOURCE)
        cls.blocks = {b['registry']['internal_name']: b for b in cls.outputs['blocks_master.json']['blocks']}
        cls.behaviours = {b['identifier'].split(':',1)[1]: set(b['tags']) for b in cls.outputs['block_behaviours.json']['blocks']}

    def test_each_block_has_one_geometry_class(self):
        for name, block in self.blocks.items():
            self.assertEqual(set(block['classification']), {'primary_geometry'}, name)
            self.assertIn(block['classification']['primary_geometry'], GEOMETRY_CLASSES, name)

    def test_gravity_is_independent_of_geometry(self):
        sand = self.blocks['sand']
        self.assertEqual(sand['classification']['primary_geometry'], 'full_cube')
        self.assertIn('gravity_affected', self.behaviours['sand'])
        self.assertNotIn('gravity_affected', GEOMETRY_CLASSES)

    def test_glowstone_and_torch_examples(self):
        self.assertEqual(self.blocks['glowstone']['classification']['primary_geometry'], 'full_cube')
        self.assertIn('light_emitter', self.behaviours['glowstone'])
        self.assertEqual(self.blocks['glowstone']['properties']['light_emission'], 15)
        self.assertEqual(self.blocks['torch']['classification']['primary_geometry'], 'partial_geometry')
        self.assertEqual(self.blocks['torch']['collision']['default_state_kind'], 'empty')
        self.assertIn('support_dependent', self.behaviours['torch'])

    def test_collision_state_mappings_are_complete(self):
        mappings = self.outputs['collision_shapes.json']['block_state_mappings']
        shapes = self.outputs['collision_shapes.json']['shapes']
        self.assertEqual(len(mappings), 1196)
        for name, block in self.blocks.items():
            lo, hi = block['registry']['min_state_id'], block['registry']['max_state_id']
            mapping = mappings['minecraft:' + name]
            self.assertEqual(set(mapping), {str(i) for i in range(lo, hi + 1)}, name)
            self.assertTrue(all(str(sid) in shapes for sid in mapping.values()), name)

    def test_block_entity_mapping_tag_category_and_statistics_agree(self):
        behaviour = self.outputs['block_behaviours.json']['blocks']
        mapped = {entry['identifier'] for entry in behaviour if entry['block_entity']['type'] is not None}
        tagged = {entry['identifier'] for entry in behaviour if 'block_entity' in entry['tags']}
        category = set(self.outputs['blocks_by_category.json']['behaviour_tags']['block_entity'])
        statistics = self.outputs['catalogue_statistics.json']['blocks_with_block_entity_metadata']
        self.assertEqual(len(mapped), 186)
        self.assertEqual(mapped, tagged)
        self.assertEqual(mapped, category)
        self.assertEqual(statistics, 186)

    def test_source_manifest_hashes_are_lf_canonical_pinned_git_blobs(self):
        self.assertEqual(canonical_lf_bytes(b'a\r\nb\rc\n'), b'a\nb\nc\n')
        manifest = self.outputs['source_manifest.json']
        self.assertEqual(manifest['line_endings'], 'LF')
        for filename, expected_blob in SOURCE_GIT_BLOBS.items():
            content = canonical_file_bytes(DEFAULT_SOURCE / filename)
            self.assertEqual(git_blob_sha1(content), expected_blob, filename)
            self.assertEqual(manifest['files'][filename]['git_blob_sha1'], expected_blob, filename)
            self.assertEqual(manifest['files'][filename]['bytes'], len(content), filename)
            self.assertEqual(manifest['files'][filename]['sha256'], hashlib.sha256(content).hexdigest(), filename)

    def test_behavior_tag_confidence_and_support_are_explicit(self):
        root = self.outputs['block_behaviours.json']
        self.assertEqual(root['tag_confidence'], BEHAVIOUR_TAG_CONFIDENCE)
        self.assertEqual({row['confidence'] for row in root['tag_confidence'].values()}, {'verified', 'inferred', 'incomplete'})
        supports = [row for row in root['blocks'] if 'support_dependent' in row['tags']]
        self.assertEqual(len(supports), 31)
        self.assertTrue(all(row['support']['confidence'] == 'incomplete' and row['support']['mechanics_complete'] is False for row in supports))

    def test_item_relationships_do_not_promote_drop_candidates(self):
        report = self.outputs['block_item_relationships.json']
        self.assertEqual(report['counts']['without_exact_name_item'], 154)
        for item in report['blocks']:
            if item['status'] == 'no_exact_name_item':
                self.assertNotIn('item', item)
                self.assertIn(item['relationship'], {'unverified', 'no_source_mapping'})

if __name__ == '__main__': unittest.main()
