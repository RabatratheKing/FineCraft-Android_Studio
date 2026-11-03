import unittest
from generate_catalogue import GEOMETRY_CLASSES, build, DEFAULT_SOURCE

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

    def test_item_relationships_do_not_promote_drop_candidates(self):
        report = self.outputs['block_item_relationships.json']
        self.assertEqual(report['counts']['without_exact_name_item'], 154)
        for item in report['blocks']:
            if item['status'] == 'no_exact_name_item':
                self.assertNotIn('item', item)
                self.assertIn(item['relationship'], {'unverified', 'no_source_mapping'})

if __name__ == '__main__': unittest.main()
